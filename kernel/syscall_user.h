#pragma once
#include <stdint.h>

static inline uint32_t syscall0(uint32_t num) {
    uint32_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num)
        : "memory"
    );
    return ret;
}

static inline uint32_t syscall1(uint32_t num, uint32_t a) {
    uint32_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(a)
        : "memory"
    );
    return ret;
}

static inline uint32_t syscall2(uint32_t num, uint32_t a, uint32_t b) {
    uint32_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(a), "c"(b)
        : "memory"
    );
    return ret;
}

static inline uint32_t syscall3(uint32_t num, uint32_t a, uint32_t b, uint32_t c) {
    uint32_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(a), "c"(b), "d"(c)
        : "memory"
    );
    return ret;
}

// Friendly wrappers
static inline void sys_exit() {
    syscall0(0);
}

static inline uint32_t sys_write(const char *str, uint32_t len, uint32_t fd) {
    return syscall3(1, (uint32_t)str, len, fd);
}

static inline uint32_t sys_read(char *buf, uint32_t len) {
    return syscall2(2, (uint32_t)buf, len);
}

static inline uint32_t sys_getpid() {
    return syscall0(3);
}

static inline void sys_sleep(uint32_t ms) {
    syscall1(4, ms);
}

static inline uint32_t sys_uptime() {
    return syscall0(5);
}

static inline uint32_t sys_create(const char *name) {
    return syscall1(8, (uint32_t)name);
}

static inline uint32_t sys_delete(const char *name) {
    return syscall1(9, (uint32_t)name);
}

