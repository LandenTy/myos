#pragma once
#include <stdint.h>
#include <stddef.h>

// ── sbrk ─────────────────────────────────────────────────────────────────────
static inline void *sbrk(int increment) {
    uint32_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(14), "b"((uint32_t)increment)
        : "memory"
    );
    return (void*)ret;
}

// ── malloc / free / realloc / calloc ─────────────────────────────────────────
// Implemented in stdlib.c — linked into every program via Makefile.
void  *malloc(size_t size);
void   free(void *ptr);
void  *calloc(size_t nmemb, size_t size);
void  *realloc(void *ptr, size_t size);

// ── atoi / itoa / abs ─────────────────────────────────────────────────────────
static inline int abs(int n) {
    return n < 0 ? -n : n;
}

static inline int atoi(const char *s) {
    int result = 0, sign = 1;
    while (*s == ' ') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9')
        result = result * 10 + (*s++ - '0');
    return sign * result;
}

// Writes integer string into buf. Returns buf.
static inline char *itoa(int n, char *buf, int base) {
    char tmp[32];
    int i = 0, neg = 0;
    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return buf; }
    if (n < 0 && base == 10) { neg = 1; n = -n; }
    unsigned int un = (unsigned int)n;
    while (un) {
        int digit = un % base;
        tmp[i++] = digit < 10 ? '0' + digit : 'a' + digit - 10;
        un /= base;
    }
    if (neg) tmp[i++] = '-';
    int j = 0;
    while (i--) buf[j++] = tmp[i];
    buf[j] = '\0';
    return buf;
}

