#pragma once
#include <stddef.h>

static inline size_t kstrlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static inline int kstrcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static inline void kmemset(void *ptr, int val, size_t n) {
    unsigned char *p = (unsigned char*)ptr;
    while (n--) *p++ = (unsigned char)val;
}

static inline void kmemcpy(void *dst, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t*)dst;
    const uint8_t *s = (const uint8_t*)src;
    while (n--) *d++ = *s++;
}

