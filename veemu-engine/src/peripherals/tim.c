/**
 * tim.c — General-purpose timer peripheral (TIM2/TIM3/TIM4/TIM5)
 *
 * Models:
 *   - Upcounting mode (default)
 *   - PSC  prescaler
 *   - ARR  auto-reload (period)
 *   - CNT  counter register
 *   - SR   status: UIF (update interrupt flag)
 *   - DIER UIE (update interrupt enable)
 *   - CR1  CEN (counter enable), ARPE, OPM
 *   - CCR1–CCR4  capture/compare registers (value stored, PWM not yet driven)
 *   - EGR  UG bit — software update event (reloads CNT=0, sets UIF)
 *
 * IRQ vector:
 *   TIM2 → IRQ 28  (NVIC position)
 *   TIM3 → IRQ 29
 *   TIM4 → IRQ 30
 *   TIM5 → IRQ 50
 *
 * engine.c calls tim_tick(p, insns) every slice to advance CNT.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../include/peripheral.h"

/* ── Register offsets (RM0368 Table 47) ─────────────────────── */
#define TIM_CR1   0x00
#define TIM_CR2   0x04
#define TIM_SMCR  0x08
#define TIM_DIER  0x0C
#define TIM_SR    0x10
#define TIM_EGR   0x14
#define TIM_CCMR1 0x18
#define TIM_CCMR2 0x1C
#define TIM_CCER  0x20
#define TIM_CNT   0x24
#define TIM_PSC   0x28
#define TIM_ARR   0x2C
#define TIM_CCR1  0x34
#define TIM_CCR2  0x38
#define TIM_CCR3  0x3C
#define TIM_CCR4  0x40
#define TIM_DCR   0x48
#define TIM_DMAR  0x4C

/* ── CR1 bits ─────────────────────────────────────────────────── */
#define CR1_CEN   (1u << 0)   /* counter enable */
#define CR1_UDIS  (1u << 1)   /* update disable */
#define CR1_OPM   (1u << 3)   /* one-pulse mode */
#define CR1_ARPE  (1u << 7)   /* auto-reload preload */

/* ── DIER bits ────────────────────────────────────────────────── */
#define DIER_UIE  (1u << 0)   /* update interrupt enable */
#define DIER_CC1IE (1u << 1)
#define DIER_CC2IE (1u << 2)
#define DIER_CC3IE (1u << 3)
#define DIER_CC4IE (1u << 4)

/* ── SR bits ──────────────────────────────────────────────────── */
#define SR_UIF    (1u << 0)   /* update interrupt flag */
#define SR_CC1IF  (1u << 1)
#define SR_CC2IF  (1u << 2)
#define SR_CC3IF  (1u << 3)
#define SR_CC4IF  (1u << 4)

typedef struct {
    uint32_t cr1, cr2, smcr, dier, sr, ccmr1, ccmr2, ccer;
    uint32_t cnt, psc, arr;
    uint32_t ccr1, ccr2, ccr3, ccr4;
    uint32_t dcr, dmar;

    /* shadow ARR — written on update event when ARPE=1 */
    uint32_t arr_shadow;

    /* prescaler accumulator — counts raw insns, fires every (PSC+1) */
    uint32_t psc_acc;

    bool     pending;   /* update event IRQ pending */
    uint8_t  timer_id;
} tim_state_t;

/* ── helpers ─────────────────────────────────────────────────── */
static void do_update_event(tim_state_t *s)
{
    s->cnt = 0;
    /* latch shadow ARR */
    if (s->cr1 & CR1_ARPE) s->arr = s->arr_shadow;
    /* set UIF if not disabled */
    if (!(s->cr1 & CR1_UDIS)) {
        s->sr |= SR_UIF;
        if (s->dier & DIER_UIE) s->pending = true;
    }
    /* one-pulse: stop counter after update */
    if (s->cr1 & CR1_OPM) s->cr1 &= ~CR1_CEN;
}

/* ── read ────────────────────────────────────────────────────── */
static uint32_t tim_read(peripheral_t *p, uint32_t offset)
{
    tim_state_t *s = p->state;
    switch (offset) {
    case TIM_CR1:   return s->cr1;
    case TIM_CR2:   return s->cr2;
    case TIM_SMCR:  return s->smcr;
    case TIM_DIER:  return s->dier;
    case TIM_SR:    return s->sr;
    case TIM_CCMR1: return s->ccmr1;
    case TIM_CCMR2: return s->ccmr2;
    case TIM_CCER:  return s->ccer;
    case TIM_CNT:   return s->cnt;
    case TIM_PSC:   return s->psc;
    case TIM_ARR:   return (s->cr1 & CR1_ARPE) ? s->arr_shadow : s->arr;
    case TIM_CCR1:  return s->ccr1;
    case TIM_CCR2:  return s->ccr2;
    case TIM_CCR3:  return s->ccr3;
    case TIM_CCR4:  return s->ccr4;
    case TIM_DCR:   return s->dcr;
    case TIM_DMAR:  return s->dmar;
    default:        return 0;
    }
}

/* ── write ───────────────────────────────────────────────────── */
static void tim_write(peripheral_t *p, uint32_t offset, uint32_t value)
{
    tim_state_t *s = p->state;
    switch (offset) {
    case TIM_CR1:
        s->cr1 = value & 0x03FF;
        break;
    case TIM_CR2:
        s->cr2 = value;
        break;
    case TIM_SMCR:
        s->smcr = value;
        break;
    case TIM_DIER:
        s->dier = value & 0x5F;
        break;
    case TIM_SR:
        /* RC_W0: bits cleared by writing 0 */
        s->sr &= value;
        break;
    case TIM_EGR:
        /* UG bit: software update event */
        if (value & 0x1) do_update_event(s);
        break;
    case TIM_CCMR1: s->ccmr1 = value; break;
    case TIM_CCMR2: s->ccmr2 = value; break;
    case TIM_CCER:  s->ccer  = value; break;
    case TIM_CNT:
        s->cnt = value;
        break;
    case TIM_PSC:
        s->psc = value & 0xFFFF;
        s->psc_acc = 0;
        break;
    case TIM_ARR:
        if (s->cr1 & CR1_ARPE)
            s->arr_shadow = value;  /* buffered */
        else
            s->arr = value;         /* immediate */
        break;
    case TIM_CCR1: s->ccr1 = value; break;
    case TIM_CCR2: s->ccr2 = value; break;
    case TIM_CCR3: s->ccr3 = value; break;
    case TIM_CCR4: s->ccr4 = value; break;
    case TIM_DCR:  s->dcr  = value; break;
    case TIM_DMAR: s->dmar = value; break;
    default: break;
    }
}

/* ── reset ───────────────────────────────────────────────────── */
static void tim_reset(peripheral_t *p)
{
    tim_state_t *s = p->state;
    memset(s, 0, sizeof(*s));
    s->arr        = 0xFFFFFFFF;  /* TIM2/TIM5 are 32-bit */
    s->arr_shadow = 0xFFFFFFFF;
}

static void tim_destroy(peripheral_t *p) { free(p->state); p->state = NULL; }

/* ── create ──────────────────────────────────────────────────── */
peripheral_t *tim_create(uint8_t timer_id)
{
    peripheral_t *p = calloc(1, sizeof(peripheral_t));
    tim_state_t  *s = calloc(1, sizeof(tim_state_t));
    if (!p || !s) { free(p); free(s); return NULL; }

    s->timer_id = timer_id;
    snprintf(p->name, sizeof(p->name), "tim%d", timer_id);
    p->type     = PERIPH_TYPE_TIMER;
    p->instance = timer_id;
    p->state    = s;
    p->read     = tim_read;
    p->write    = tim_write;
    p->reset    = tim_reset;
    p->destroy  = tim_destroy;
    tim_reset(p);
    return p;
}

/**
 * tim_tick — called by engine.c every instruction slice
 *
 * @param p            peripheral pointer
 * @param insns        instructions executed this slice
 * @param cpu_hz       processor clock (e.g. 16_000_000)
 *
 * Returns true if an update event fired (UIE pending → engine injects IRQ).
 *
 * Prescaler: CNT increments every (PSC+1) cpu ticks.
 * We approximate: insns ≈ cpu ticks at 1 IPC.
 */
bool tim_tick(peripheral_t *p, uint32_t insns, uint32_t cpu_hz)
{
    (void)cpu_hz;
    tim_state_t *s = p->state;
    if (!(s->cr1 & CR1_CEN)) return false;
    if (s->arr == 0)          return false;

    uint32_t period = s->psc + 1;
    s->psc_acc += insns;

    /* how many timer ticks elapsed this slice */
    uint32_t ticks = s->psc_acc / period;
    s->psc_acc    %= period;
    if (ticks == 0) return false;

    bool fired = false;

/* ticks until next update event */
    uint32_t to_overflow = s->arr + 1 - s->cnt;

    if (ticks < to_overflow) {
        /* no overflow this slice — just advance CNT */
        s->cnt += ticks;
        return false;
    }

    /* overflow: fire exactly one update event.
     * Bank leftover ticks back as insns for next call. */
    uint32_t leftover = ticks - to_overflow;
    s->psc_acc += leftover * period;

    do_update_event(s);

    return (s->dier & DIER_UIE) != 0;
}

bool tim_is_pending(peripheral_t *p)
{
    tim_state_t *s = p->state;
    if (s->pending) { s->pending = false; return true; }
    return false;
}
