/**
 * syscalls.c — Newlib retargets + malloc-free print library
 *
 * printf() — minimal implementation, no malloc, no _dtoa_r, no vsnprintf.
 *             Supports: %s %c %d %i %u %lu %ld %x %X %08x %f %.Nf %%
 * uart_print/println/print_u32/print_hex — direct _write, always safe.
 * puts() — newlib weak override, direct _write.
 *
 * Zero heap usage from print functions — no FETCH_UNMAPPED crashes.
 */

#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include "platform_config.h"

#define SR_TXE (1u << 7)

/* ── Single byte to UART ─────────────────────────────────────────── */
static void _uart_putc_raw(char c)
{
    while (!(PLATFORM_UART_SR & SR_TXE)) {}
    PLATFORM_UART_DR = (uint32_t)(uint8_t)c;
}

/* ── _write — \n then \r ─────────────────────────────────────────── */
int _write(int fd, char *buf, int len)
{
    (void)fd;
    if (!buf || len <= 0) return 0;
    for (int i = 0; i < len; i++) {
        _uart_putc_raw(buf[i]);
        if (buf[i] == '\n') _uart_putc_raw('\r');
    }
    return len;
}

/* ── printf helpers — all stack-only, zero malloc ────────────────── */
static void _print_uint(uint32_t v, int base, int upper)
{
    const char *lo = "0123456789abcdef";
    const char *hi = "0123456789ABCDEF";
    const char *d  = upper ? hi : lo;
    char buf[16]; int i = 0;
    if (v == 0) { _uart_putc_raw('0'); return; }
    while (v) { buf[i++] = d[v % base]; v /= base; }
    while (i > 0) _uart_putc_raw(buf[--i]);
}

static void _print_int(int32_t v)
{
    if (v < 0) { _uart_putc_raw('-'); _print_uint((uint32_t)-v, 10, 0); }
    else _print_uint((uint32_t)v, 10, 0);
}

static void _print_hex_width(uint32_t v, int width, char pad, int upper)
{
    const char *lo = "0123456789abcdef";
    const char *hi = "0123456789ABCDEF";
    const char *d  = upper ? hi : lo;
    char buf[12]; int i = 0;
    if (v == 0 && width == 0) { _uart_putc_raw('0'); return; }
    uint32_t t = v;
    while (t) { buf[i++] = d[t % 16]; t /= 16; }
    while (i < width) buf[i++] = pad;
    while (i > 0) _uart_putc_raw(buf[--i]);
}

static void _print_float(double v, int prec)
{
    if (v < 0.0) { _uart_putc_raw('-'); v = -v; }
    uint32_t ipart = (uint32_t)v;
    _print_uint(ipart, 10, 0);
    _uart_putc_raw('.');
    double fpart = v - (double)ipart;
    for (int p = 0; p < prec; p++) {
        fpart *= 10.0;
        int digit = (int)fpart;
        _uart_putc_raw('0' + digit);
        fpart -= digit;
    }
}

/* ── printf ──────────────────────────────────────────────────────── */
int printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int written = 0;

    while (*fmt) {
        if (*fmt != '%') {
            _uart_putc_raw(*fmt++);
            written++;
            continue;
        }
        fmt++;

        /* pad/width/precision */
        char pad = ' '; int width = 0; int prec = 6;
        if (*fmt == '0') { pad = '0'; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') { width = width*10 + (*fmt-'0'); fmt++; }
        if (*fmt == '.') {
            fmt++; prec = 0;
            while (*fmt >= '0' && *fmt <= '9') { prec = prec*10 + (*fmt-'0'); fmt++; }
        }

        /* length */
        int is_long = 0;
        if (*fmt == 'l') { is_long = 1; fmt++; }

        switch (*fmt++) {
        case 's': {
            const char *s = va_arg(args, const char *);
            if (!s) s = "(null)";
            while (*s) { _uart_putc_raw(*s++); written++; }
            break;
        }
        case 'c':
            _uart_putc_raw((char)va_arg(args, int));
            written++;
            break;
        case 'd': case 'i':
            if (is_long) _print_int((int32_t)va_arg(args, long));
            else         _print_int((int32_t)va_arg(args, int));
            written++;
            break;
        case 'u':
            if (is_long) _print_uint((uint32_t)va_arg(args, unsigned long), 10, 0);
            else         _print_uint((uint32_t)va_arg(args, unsigned int),  10, 0);
            written++;
            break;
        case 'x':
            _print_hex_width(
                is_long ? (uint32_t)va_arg(args, unsigned long)
                        : (uint32_t)va_arg(args, unsigned int),
                width, pad, 0);
            written++;
            break;
        case 'X':
            _print_hex_width(
                is_long ? (uint32_t)va_arg(args, unsigned long)
                        : (uint32_t)va_arg(args, unsigned int),
                width, pad, 1);
            written++;
            break;
        case 'f': case 'F':
            _print_float(va_arg(args, double), prec);
            written++;
            break;
        case '%':
            _uart_putc_raw('%');
            written++;
            break;
        default:
            break;
        }
    }
    va_end(args);
    return written;
}

/* ── puts override ───────────────────────────────────────────────── */
int puts(const char *s)
{
    int len = (int)strlen(s);
    _write(1, (char *)s, len);
    _write(1, "\n", 1);
    return len;
}

/* ── uart_print API ──────────────────────────────────────────────── */
void uart_putc(char c)           { _write(1, &c, 1); }
void uart_print(const char *s)   { if (s) _write(1, (char*)s, (int)strlen(s)); }
void uart_println(const char *s) { uart_print(s); _write(1, "\n", 1); }

void uart_print_u32(uint32_t v)
{
    char buf[12]; int i = 0;
    if (v == 0) { _uart_putc_raw('0'); return; }
    while (v) { buf[i++] = '0' + (v % 10); v /= 10; }
    while (i > 0) _uart_putc_raw(buf[--i]);
}

void uart_print_hex(uint32_t v)
{
    const char hex[] = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--)
        _uart_putc_raw(hex[(v >> (i * 4)) & 0xF]);
}

/* ── Newlib stubs ────────────────────────────────────────────────── */
int _read(int fd, char *buf, int len)      { (void)fd;(void)buf;(void)len; return 0; }
int _close(int fd)                         { (void)fd; return -1; }
int _fstat(int fd, struct stat *st)        { (void)fd; st->st_mode = S_IFCHR; return 0; }
int _isatty(int fd)                        { (void)fd; return 1; }
int _lseek(int fd, int off, int w)         { (void)fd;(void)off;(void)w; return 0; }
void _exit(int status)                     { (void)status; while(1){} }
int _kill(int pid, int sig)                { (void)pid;(void)sig; return -1; }
int _getpid(void)                          { return 1; }

/* ── _sbrk — kept for any other malloc users ─────────────────────── */
extern uint32_t _ebss;
extern uint32_t _estack;

void *_sbrk(int incr)
{
    static uint8_t *heap_end = NULL;
    if (heap_end == NULL) heap_end = (uint8_t *)&_ebss;
    if ((uint32_t)(heap_end + incr) > (uint32_t)&_estack) return (void *)-1;
    uint8_t *prev = heap_end;
    heap_end += incr;
    return prev;
}