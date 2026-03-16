#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "unicorn/unicorn.h"
#include "../include/veemu.h"
#include "../include/peripheral.h"

peripheral_t *uart_create(uint8_t id);
peripheral_t *rcc_create(void);
peripheral_t *gpio_create(uint8_t idx);
peripheral_t *systick_create(void);
peripheral_t *tim_create(uint8_t timer_id);
peripheral_t *i2c_create(uint8_t i2c_id);
peripheral_t *spi_create(uint8_t spi_id);
void    uart_set_tx_cb(peripheral_t*,veemu_uart_tx_cb_t,void*);
bool    uart_rx_inject(peripheral_t*,uint8_t);
size_t  uart_tx_drain(peripheral_t*,uint8_t*,size_t);
void    gpio_set_input_pin(peripheral_t*,uint8_t,bool);
void    gpio_set_cb(peripheral_t*,veemu_gpio_cb_t,void*);
bool    systick_tick(peripheral_t*,uint32_t);
bool    systick_is_pending(peripheral_t*);
bool    tim_tick(peripheral_t*,uint32_t,uint32_t);
bool    tim_is_pending(peripheral_t*);
uint32_t elf_load(void*,const char*,uint32_t,uint32_t,uint32_t,uint32_t);

#define MAX_PERIPHERALS 32
#define MAX_TIMERS      4
#define SLICE_INSNS     1000u

struct veemu_board {
    uc_engine   *uc;
    uint32_t     flash_base,flash_size,sram_base,sram_size,entry;
    peripheral_t *periph[MAX_PERIPHERALS];
    int           periph_count;
    peripheral_t *systick;
    peripheral_t *timers[MAX_TIMERS];
    int           timer_count;
    uint32_t      cpu_hz;
    uint8_t       tim_exc[8];
    veemu_uart_tx_cb_t uart_tx_cb; void *uart_tx_ctx;
    veemu_gpio_cb_t    gpio_cb;    void *gpio_ctx;
};

static bool g_exc_return_hit = false;

static void exc_return_hook(uc_engine *uc, uint64_t addr,
                             uint32_t size, void *user)
{
    (void)size; (void)user;
    if ((addr & 0xFFFFFFF0u) == 0xFFFFFFF0u) {
        g_exc_return_hit = true;
        uc_emu_stop(uc);
    }
}

/* ── Peripheral lookup ───────────────────────────────────────────────── */
static peripheral_t *find_periph_by_addr(veemu_board_t *b, uint32_t a) {
    for (int i = 0; i < b->periph_count; i++) {
        peripheral_t *p = b->periph[i];
        if (a >= p->base_addr && a < p->base_addr + p->size)
            return p;
    }
    return NULL;
}

/* ── Guards (defined before both hooks that use them) ───────────────── */
static int g_in_periph_write = 0;
static int g_in_read_sync    = 0;

/* ── Read hook: fires BEFORE Unicorn reads — pre-load peripheral model ──
 * UC_HOOK_MEM_READ fires before the CPU reads the value, so we write the
 * model value into Unicorn memory first. The CPU then reads the correct value.
 * This replaces READ_AFTER which was too late — firmware already got stale data. */
static void mem_read_hook(uc_engine *uc, uc_mem_type type,
                           uint64_t addr, int size, int64_t value, void *user) {
    (void)type; (void)size; (void)value;
    veemu_board_t *b = (veemu_board_t*)user;
    uint32_t a = (uint32_t)addr;
    peripheral_t *p = find_periph_by_addr(b, a);
    if (!p) return;
    uint32_t off = a - p->base_addr;
    uint32_t v = p->read(p, off);
    g_in_read_sync = 1;
    uc_mem_write(uc, addr, &v, 4);
    g_in_read_sync = 0;
}

/* ── Peripheral write handler — shared by both write hooks ──────────── */
static void periph_write(uc_engine *uc, veemu_board_t *b,
                         uint64_t addr, int64_t val)
{
    if (g_in_periph_write) return;  /* re-entrancy guard */
    if (g_in_read_sync)    return;  /* ignore uc_mem_write from read_after_hook */
    uint32_t a = (uint32_t)addr;
    peripheral_t *p = find_periph_by_addr(b, a);
    if (!p) return;
    g_in_periph_write = 1;
    uint32_t off = a - p->base_addr;
    p->write(p, off, (uint32_t)val);
    g_in_periph_write = 0;
}

/* ── Write-prot hook ─────────────────────────────────────────────────── */
static bool mem_write_prot_hook(uc_engine *uc, uc_mem_type type,
                                 uint64_t addr, int size, int64_t val, void *user) {
    (void)type; (void)size;
    periph_write(uc, (veemu_board_t*)user, addr, val);
    return true;
}

/* ── Write hook: Unicorn 2.x ─────────────────────────────────────────── */
static void mem_write_hook(uc_engine *uc, uc_mem_type type,
                            uint64_t addr, int size, int64_t val, void *user) {
    (void)type; (void)size;
    periph_write(uc, (veemu_board_t*)user, addr, val);
}

/* ── Invalid hook: catches unmapped accesses ────────────────────────── */
static bool mem_invalid_hook(uc_engine *uc, uc_mem_type type,
                              uint64_t addr, int size, int64_t val, void *user) {
    (void)uc; (void)size; (void)val; (void)user;
    const char *t = "UNKNOWN";
    switch (type) {
    case UC_MEM_READ_UNMAPPED:  t = "READ_UNMAPPED";  break;
    case UC_MEM_WRITE_UNMAPPED: t = "WRITE_UNMAPPED"; break;
    case UC_MEM_FETCH_UNMAPPED: t = "FETCH_UNMAPPED"; break;
    default: break;
    }
    fprintf(stderr, "[veemu] FAULT: %s @ 0x%08X\n", t, (uint32_t)addr);
    return false;
}

static uint32_t jget_hex(const char *j, const char *key) {
    char s[64]; snprintf(s, sizeof(s), "\"%s\"", key);
    char *p = strstr(j, s); if (!p) return 0;
    p += strlen(s);
    while (*p == ' ' || *p == ':' || *p == '\t') p++;
    if (*p == '"') p++;
    return (uint32_t)strtoul(p, NULL, 0);
}
static uint32_t jget_uint(const char *j, const char *key) {
    char s[64]; snprintf(s, sizeof(s), "\"%s\"", key);
    char *p = strstr(j, s); if (!p) return 0;
    p += strlen(s);
    while (*p == ' ' || *p == ':' || *p == '\t') p++;
    return (uint32_t)strtoul(p, NULL, 0);
}

static void reg_periph(veemu_board_t *b, peripheral_t *p, uint32_t base, uint32_t size) {
    if (b->periph_count >= MAX_PERIPHERALS) return;
    p->base_addr = base; p->size = size; p->board = b;
    b->periph[b->periph_count++] = p;
    /*
     * Map peripheral as READ-ONLY.
     * Unicorn requires page-aligned (4096) base and size.
     * Round base down and size up to nearest page boundary.
     * Only map if the page is not already mapped (overlapping peripherals
     * share a page — e.g. I2C1/I2C2/I2C3 all share 0x40005000 page).
     */
    uint32_t page      = 0x1000;
    uint32_t map_base  = base & ~(page - 1);
    uint32_t map_end   = (base + size + page - 1) & ~(page - 1);
    uint32_t map_size  = map_end - map_base;
    uc_err e = uc_mem_map(b->uc, map_base, map_size, UC_PROT_ALL);
    fprintf(stderr, "[veemu] mmap 0x%08X+%u -> %s\n", map_base, map_size, uc_strerror(e));
    if (e != UC_ERR_OK && e != UC_ERR_MAP) {
        fprintf(stderr, "[veemu] WARNING: uc_mem_map 0x%08X+%u: %s\n",
                map_base, map_size, uc_strerror(e));
    }
    /* UC_ERR_MAP means already mapped — that is fine for shared pages */
}

static void reg_timer(veemu_board_t *b, peripheral_t *p, uint32_t base, uint32_t size) {
    reg_periph(b, p, base, size);
    if (b->timer_count < MAX_TIMERS)
        b->timers[b->timer_count++] = p;
}

static peripheral_t *find_periph(veemu_board_t *b, const char *name) {
    for (int i = 0; i < b->periph_count; i++)
        if (strcmp(b->periph[i]->name, name) == 0) return b->periph[i];
    return NULL;
}

/* ── Exception injection (ARMv7-M §B1.5) ────────────────────────────── */
static void inject_exception(veemu_board_t *b, uint8_t vector_num) {
    uc_engine *uc = b->uc;
    uint32_t sp=0,r0=0,r1=0,r2=0,r3=0,r12=0,lr=0,pc=0,xpsr=0;
    uc_reg_read(uc, UC_ARM_REG_SP,   &sp);
    uc_reg_read(uc, UC_ARM_REG_R0,   &r0);
    uc_reg_read(uc, UC_ARM_REG_R1,   &r1);
    uc_reg_read(uc, UC_ARM_REG_R2,   &r2);
    uc_reg_read(uc, UC_ARM_REG_R3,   &r3);
    uc_reg_read(uc, UC_ARM_REG_R12,  &r12);
    uc_reg_read(uc, UC_ARM_REG_LR,   &lr);
    uc_reg_read(uc, UC_ARM_REG_PC,   &pc);
    uc_reg_read(uc, UC_ARM_REG_XPSR, &xpsr);

    sp = (sp - 32) & ~7u;
    uc_mem_write(uc, sp+ 0, &r0,   4);
    uc_mem_write(uc, sp+ 4, &r1,   4);
    uc_mem_write(uc, sp+ 8, &r2,   4);
    uc_mem_write(uc, sp+12, &r3,   4);
    uc_mem_write(uc, sp+16, &r12,  4);
    uc_mem_write(uc, sp+20, &lr,   4);
    uc_mem_write(uc, sp+24, &pc,   4);
    uint32_t frame_xpsr = (xpsr & ~0x1FFu) | 0x01000000u;
    uc_mem_write(uc, sp+28, &frame_xpsr, 4);
    uc_reg_write(uc, UC_ARM_REG_SP, &sp);

    uint32_t vec_addr = b->flash_base + vector_num * 4;
    uint32_t handler = 0;
    uc_mem_read(uc, vec_addr, &handler, 4);
    uint32_t handler_pc = handler & ~1u;
    uc_reg_write(uc, UC_ARM_REG_PC, &handler_pc);

    uint32_t exc_ret = 0xFFFFFFF9u;
    uc_reg_write(uc, UC_ARM_REG_LR, &exc_ret);

    uint32_t new_xpsr = (xpsr & ~0x1FFu) | vector_num;
    uc_reg_write(uc, UC_ARM_REG_XPSR, &new_xpsr);
}

static void restore_exception(veemu_board_t *b) {
    uc_engine *uc = b->uc;
    uint32_t sp = 0;
    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    uint32_t r0=0,r1=0,r2=0,r3=0,r12=0,lr=0,pc=0,xpsr=0;
    uc_mem_read(uc, sp+ 0, &r0,   4);
    uc_mem_read(uc, sp+ 4, &r1,   4);
    uc_mem_read(uc, sp+ 8, &r2,   4);
    uc_mem_read(uc, sp+12, &r3,   4);
    uc_mem_read(uc, sp+16, &r12,  4);
    uc_mem_read(uc, sp+20, &lr,   4);
    uc_mem_read(uc, sp+24, &pc,   4);
    uc_mem_read(uc, sp+28, &xpsr, 4);
    sp += 32;
    uc_reg_write(uc, UC_ARM_REG_SP,   &sp);
    uc_reg_write(uc, UC_ARM_REG_PC,   &pc);
    uc_reg_write(uc, UC_ARM_REG_XPSR, &xpsr);
    uc_reg_write(uc, UC_ARM_REG_R0,   &r0);
    uc_reg_write(uc, UC_ARM_REG_R1,   &r1);
    uc_reg_write(uc, UC_ARM_REG_R2,   &r2);
    uc_reg_write(uc, UC_ARM_REG_R3,   &r3);
    uc_reg_write(uc, UC_ARM_REG_R12,  &r12);
    uc_reg_write(uc, UC_ARM_REG_LR,   &lr);
}

veemu_err_t veemu_board_create(const char *path, veemu_board_t **out) {
    FILE *f = fopen(path, "r"); if (!f) return VEEMU_ERR_BOARD;
    fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f);
    char *j = malloc((size_t)sz + 1); fread(j, 1, (size_t)sz, f); j[sz] = 0; fclose(f);

    char *flash_obj = strstr(j, "\"flash\""), *sram_obj = strstr(j, "\"sram\"");
    if (!flash_obj || !sram_obj) { free(j); return VEEMU_ERR_BOARD; }
    uint32_t fb = jget_hex(flash_obj+7, "base"), fs = jget_uint(flash_obj+7, "size");
    uint32_t sb = jget_hex(sram_obj+6,  "base"), ss = jget_uint(sram_obj+6,  "size");
    fprintf(stderr, "[veemu] flash=0x%08X+%uK sram=0x%08X+%uK\n", fb, fs/1024, sb, ss/1024);

    veemu_board_t *b = calloc(1, sizeof(veemu_board_t));
    if (!b) { free(j); return VEEMU_ERR_NOMEM; }
    b->flash_base = fb; b->flash_size = fs;
    b->sram_base  = sb; b->sram_size  = ss;

    uc_err ue = uc_open(UC_ARCH_ARM, UC_MODE_THUMB|UC_MODE_MCLASS, &b->uc);
    if (ue != UC_ERR_OK) {
        fprintf(stderr, "[veemu] uc_open: %s\n", uc_strerror(ue));
        free(b); free(j); return VEEMU_ERR_UC;
    }

    /* Memory layout:
     * 0x00000000 : scratch page (absorbs Unicorn internal writes)
     * flash_base : firmware flash (read+exec)
     * sram_base  : SRAM (read+write)
     * peripherals: registered read-only via reg_periph
     * 0xE0000000 : Cortex-M core peripherals (SysTick etc) read-only
     */
    uc_mem_map(b->uc, 0x00000000, 0x1000,    UC_PROT_ALL);
    uc_mem_map(b->uc, fb,         fs,         UC_PROT_READ|UC_PROT_EXEC);
    uc_mem_map(b->uc, sb,         ss,         UC_PROT_READ|UC_PROT_WRITE);
    uc_mem_map(b->uc, 0xE0000000, 0x100000,   UC_PROT_ALL);

    uc_hook hh;
    /* READ: pre-load peripheral model into Unicorn memory BEFORE CPU reads.
     * UC_HOOK_MEM_READ fires before the read so firmware gets correct value. */
    uc_hook_add(b->uc, &hh, UC_HOOK_MEM_READ,
                mem_read_hook, b, 0x40000000, 0x60000000);
    uc_hook_add(b->uc, &hh, UC_HOOK_MEM_READ,
                mem_read_hook, b, 0xE0000000, 0xE0100000);
    /* WRITE_PROT: intercept writes to read-only peripheral regions.
     * Unicorn 2.x: UC_HOOK_MEM_WRITE fires for ALL writes including to
     * UC_PROT_READ regions. UC_HOOK_MEM_WRITE_PROT may not fire in 2.x.
     * Register both to support Unicorn 1.x and 2.x. */
    /* Unicorn 2.x: MEM_WRITE fires for UC_PROT_READ regions.
     * Range covers all peripheral addresses 0x40000000-0x60000000.
     * Also covers core peripherals via second hook below. */
    uc_hook_add(b->uc, &hh, UC_HOOK_MEM_WRITE,
                mem_write_hook, b, 0x40000000, 0x60000000);
    uc_hook_add(b->uc, &hh, UC_HOOK_MEM_WRITE,
                mem_write_hook, b, 0xE0000000, 0xE0100000);
    /* INVALID: catch truly unmapped accesses */
    uc_hook_add(b->uc, &hh, UC_HOOK_MEM_INVALID,
                mem_invalid_hook, b, 1, 0);

    /* RCC */
    char *o = strstr(j, "\"rcc\"");
    if (o) {
        uint32_t base = jget_hex(o+5, "base"), size = jget_uint(o+5, "size");
        reg_periph(b, rcc_create(), base, size);
        fprintf(stderr, "[veemu] rcc @ 0x%08X\n", base);
    }

    /* GPIO */
    const char *gk[] = {"gpioa","gpiob","gpioc","gpiod","gpioe","gpioh"};
    uint8_t gi[] = {0,1,2,3,4,7};
    for (int i = 0; i < 6; i++) {
        char s[16]; snprintf(s, sizeof(s), "\"%s\"", gk[i]);
        char *p = strstr(j, s); if (!p) continue;
        uint32_t base = jget_hex(p+strlen(s), "base"), size = jget_uint(p+strlen(s), "size");
        reg_periph(b, gpio_create(gi[i]), base, size);
        fprintf(stderr, "[veemu] %s @ 0x%08X\n", gk[i], base);
    }

    /* USART */
    const char *uk[] = {"usart1","usart2","usart6"};
    uint8_t uid[] = {1,2,6};
    for (int i = 0; i < 3; i++) {
        char s[16]; snprintf(s, sizeof(s), "\"%s\"", uk[i]);
        char *p = strstr(j, s); if (!p) continue;
        uint32_t base = jget_hex(p+strlen(s), "base"), size = jget_uint(p+strlen(s), "size");
        reg_periph(b, uart_create(uid[i]), base, size);
        fprintf(stderr, "[veemu] %s @ 0x%08X\n", uk[i], base);
    }

    /* SysTick */
    {
        peripheral_t *st = systick_create();
        reg_periph(b, st, 0xE000E010, 16);
        b->systick = st;
        fprintf(stderr, "[veemu] systick @ 0xE000E010\n");
    }

    /* CPU clock */
    {
        char *ck = strstr(j, "\"clock\"");
        b->cpu_hz = ck ? jget_uint(ck+7, "hsi_hz") : 16000000u;
        if (b->cpu_hz == 0) b->cpu_hz = 16000000u;
        fprintf(stderr, "[veemu] cpu_hz=%u\n", b->cpu_hz);
    }

    /* TIM IRQ map */
    {
        struct { const char *key; uint8_t id; } tirqs[] = {
            {"tim2",2},{"tim3",3},{"tim4",4},{"tim5",5}
        };
        char *intr = strstr(j, "\"interrupts\"");
        for (int i = 0; i < (int)(sizeof(tirqs)/sizeof(tirqs[0])); i++) {
            if (!intr) {
                uint8_t def[] = {0,0,28,29,30,50};
                if (tirqs[i].id < sizeof(def))
                    b->tim_exc[tirqs[i].id] = def[tirqs[i].id] + 16u;
                continue;
            }
            char s[16]; snprintf(s, sizeof(s), "\"%s\"", tirqs[i].key);
            char *p = strstr(intr, s); if (!p) continue;
            uint32_t irq = jget_uint(p+strlen(s), "irq");
            if (irq) b->tim_exc[tirqs[i].id] = (uint8_t)(irq + 16u);
        }
    }

    /* TIMers */
    {
        struct { const char *key; uint8_t id; } tims[] = {
            {"tim1",1},{"tim2",2},{"tim3",3},{"tim4",4},{"tim5",5}
        };
        for (int i = 0; i < (int)(sizeof(tims)/sizeof(tims[0])); i++) {
            char s[16]; snprintf(s, sizeof(s), "\"%s\"", tims[i].key);
            char *p = strstr(j, s); if (!p) continue;
            uint32_t base = jget_hex(p+strlen(s), "base");
            uint32_t size = jget_uint(p+strlen(s), "size");
            if (!base || !size) continue;
            peripheral_t *tp = tim_create(tims[i].id);
            reg_timer(b, tp, base, size);
            fprintf(stderr, "[veemu] %s @ 0x%08X\n", tims[i].key, base);
        }
    }

    /* SPI */
    {
        struct { const char *key; uint8_t id; } spis[] = {
            {"spi1",1},{"spi2",2},{"spi3",3}
        };
        for (int i = 0; i < (int)(sizeof(spis)/sizeof(spis[0])); i++) {
            char s[16]; snprintf(s, sizeof(s), "\"%s\"", spis[i].key);
            char *p = strstr(j, s); if (!p) continue;
            uint32_t base = jget_hex(p+strlen(s), "base");
            uint32_t size = jget_uint(p+strlen(s), "size");
            if (!base || !size) continue;
            reg_periph(b, spi_create(spis[i].id), base, size);
            fprintf(stderr, "[veemu] %s @ 0x%08X\n", spis[i].key, base);
        }
    }

    /* I2C */
    {
        struct { const char *key; uint8_t id; } i2cs[] = {
            {"i2c1",1},{"i2c2",2},{"i2c3",3}
        };
        for (int i = 0; i < (int)(sizeof(i2cs)/sizeof(i2cs[0])); i++) {
            char s[16]; snprintf(s, sizeof(s), "\"%s\"", i2cs[i].key);
            char *p = strstr(j, s); if (!p) continue;
            uint32_t base = jget_hex(p+strlen(s), "base");
            uint32_t size = jget_uint(p+strlen(s), "size");
            if (!base || !size) continue;
            reg_periph(b, i2c_create(i2cs[i].id), base, size);
            fprintf(stderr, "[veemu] %s @ 0x%08X\n", i2cs[i].key, base);
        }
    }

    free(j); *out = b; return VEEMU_OK;
}

veemu_err_t veemu_board_load_elf(veemu_board_t *b, const char *path) {
    uint32_t e = elf_load(b->uc, path, b->flash_base, b->flash_size,
                          b->sram_base, b->sram_size);
    if (!e) return VEEMU_ERR_ELF;
    b->entry = e;
    return VEEMU_OK;
}

veemu_err_t veemu_board_reset(veemu_board_t *b) {
    uint32_t sp = 0, rv = 0;
    uc_mem_read(b->uc, b->flash_base+0, &sp, 4);
    uc_mem_read(b->uc, b->flash_base+4, &rv, 4);
    fprintf(stderr, "[veemu] reset SP=0x%08X PC=0x%08X\n", sp, rv&~1u);

    uc_reg_write(b->uc, UC_ARM_REG_SP,  &sp);
    uc_reg_write(b->uc, UC_ARM_REG_MSP, &sp);
    uint32_t pc = rv & ~1u;
    uc_reg_write(b->uc, UC_ARM_REG_PC,  &pc);
    /* Set Thumb bit in CPSR */
    uint32_t cpsr = 0;
    uc_reg_read(b->uc, UC_ARM_REG_CPSR, &cpsr);
    cpsr |= (1u << 5);
    uc_reg_write(b->uc, UC_ARM_REG_CPSR, &cpsr);

    /* Reset all peripherals */
    for (int i = 0; i < b->periph_count; i++)
        if (b->periph[i]->reset) b->periph[i]->reset(b->periph[i]);

    return VEEMU_OK;
}

veemu_run_result_t veemu_board_run(veemu_board_t *b, uint64_t max_insn) {
    uint64_t total = 0;

    while (max_insn == 0 || total < max_insn) {
        uint64_t slice = SLICE_INSNS;
        if (max_insn > 0 && total + slice > max_insn) slice = max_insn - total;

        uint32_t pc = 0;
        uc_reg_read(b->uc, UC_ARM_REG_PC, &pc);

        uc_err err = uc_emu_start(b->uc, pc|1, 0xFFFFFFFF, 0, slice);
        total += slice;

        if (err != UC_ERR_OK) {
            if (err == UC_ERR_INSN_INVALID) return VEEMU_RUN_HALTED;
            fprintf(stderr, "[veemu] run error: %s\n", uc_strerror(err));
            return VEEMU_RUN_FAULT;
        }

        /* SysTick */
        if (b->systick && systick_tick(b->systick, (uint32_t)slice)) {
            uc_hook hh;
            g_exc_return_hit = false;
            uc_hook_add(b->uc, &hh, UC_HOOK_CODE,
                        exc_return_hook, NULL, 0xFFFFFFF0ULL, 0xFFFFFFFFULL);
            inject_exception(b, 15);
            uint32_t hpc = 0;
            uc_reg_read(b->uc, UC_ARM_REG_PC, &hpc);
            uc_emu_start(b->uc, hpc|1, 0xFFFFFFFF, 0, 20000);
            uc_hook_del(b->uc, hh);
            restore_exception(b);
        }

        /* TIMers */
        for (int ti = 0; ti < b->timer_count; ti++) {
            peripheral_t *tp = b->timers[ti];
            if (!tp) continue;
            if (tim_tick(tp, (uint32_t)slice, b->cpu_hz)) {
                uint8_t tid = tp->instance;
                uint8_t exc = (tid < 8) ? b->tim_exc[tid] : 0;
                if (exc == 0) continue;
                uc_hook hh2;
                g_exc_return_hit = false;
                uc_hook_add(b->uc, &hh2, UC_HOOK_CODE,
                            exc_return_hook, NULL, 0xFFFFFFF0ULL, 0xFFFFFFFFULL);
                inject_exception(b, exc);
                uint32_t hpc2 = 0;
                uc_reg_read(b->uc, UC_ARM_REG_PC, &hpc2);
                uc_emu_start(b->uc, hpc2|1, 0xFFFFFFFF, 0, 20000);
                uc_hook_del(b->uc, hh2);
                restore_exception(b);
            }
        }
    }
    return VEEMU_RUN_OK;
}

void veemu_set_uart_tx_cb(veemu_board_t *b, veemu_uart_tx_cb_t cb, void *ctx) {
    b->uart_tx_cb = cb; b->uart_tx_ctx = ctx;
    for (int i = 0; i < b->periph_count; i++)
        if (b->periph[i]->type == PERIPH_TYPE_UART)
            uart_set_tx_cb(b->periph[i], cb, ctx);
}
void veemu_set_gpio_cb(veemu_board_t *b, veemu_gpio_cb_t cb, void *ctx) {
    b->gpio_cb = cb; b->gpio_ctx = ctx;
    for (int i = 0; i < b->periph_count; i++)
        if (b->periph[i]->type == PERIPH_TYPE_GPIO)
            gpio_set_cb(b->periph[i], cb, ctx);
}
veemu_err_t veemu_uart_rx_inject(veemu_board_t *b, uint8_t uid, uint8_t byte) {
    char n[16]; snprintf(n, sizeof(n), "usart%d", uid);
    peripheral_t *p = find_periph(b, n); if (!p) return VEEMU_ERR_PARAM;
    uart_rx_inject(p, byte); return VEEMU_OK;
}
size_t veemu_uart_drain(veemu_board_t *b, uint8_t uid, uint8_t *buf, size_t len) {
    char n[16]; snprintf(n, sizeof(n), "usart%d", uid);
    peripheral_t *p = find_periph(b, n); if (!p) return 0;
    return uart_tx_drain(p, buf, len);
}
veemu_err_t veemu_gpio_set_input(veemu_board_t *b, const char *port, uint8_t pin, bool state) {
    peripheral_t *p = find_periph(b, port); if (!p) return VEEMU_ERR_PARAM;
    gpio_set_input_pin(p, pin, state); return VEEMU_OK;
}
uint32_t veemu_reg_read(veemu_board_t *b, uint32_t addr) {
    uint32_t v = 0; uc_mem_read(b->uc, addr, &v, 4); return v;
}
const char *veemu_strerror(veemu_err_t e) {
    switch (e) {
    case VEEMU_OK:        return "OK";
    case VEEMU_ERR_NOMEM: return "Out of memory";
    case VEEMU_ERR_ELF:   return "ELF load failed";
    case VEEMU_ERR_BOARD: return "Board config error";
    case VEEMU_ERR_UC:    return "Unicorn error";
    case VEEMU_ERR_IO:    return "I/O error";
    case VEEMU_ERR_PARAM: return "Invalid parameter";
    default:              return "Unknown";
    }
}
void veemu_board_free(veemu_board_t *b) {
    if (!b) return;
    for (int i = 0; i < b->periph_count; i++) {
        if (b->periph[i]->destroy) b->periph[i]->destroy(b->periph[i]);
        free(b->periph[i]);
    }
    uc_close(b->uc); free(b);
}
