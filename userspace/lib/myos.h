#pragma once
#include <stdint.h>

#define STDIN   0
#define STDOUT  1
#define STDERR  2

// ── Raw syscall stubs ────────────────────────────────────────────────────────

static inline uint32_t syscall0(uint32_t num) {
    uint32_t ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num) : "memory");
    return ret;
}
static inline uint32_t syscall1(uint32_t num, uint32_t a) {
    uint32_t ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(a) : "memory");
    return ret;
}
static inline uint32_t syscall2(uint32_t num, uint32_t a, uint32_t b) {
    uint32_t ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(a), "c"(b) : "memory");
    return ret;
}
static inline uint32_t syscall3(uint32_t num, uint32_t a, uint32_t b, uint32_t c) {
    uint32_t ret;
    __asm__ volatile ("int $0x80" : "=a"(ret) : "a"(num), "b"(a), "c"(b), "d"(c) : "memory");
    return ret;
}

// ── Process syscalls ─────────────────────────────────────────────────────────

static inline void exit() {
    syscall0(0);
}
static inline uint32_t getpid() {
    return syscall0(3);
}
static inline void sleep(uint32_t ms) {
    syscall1(4, ms);
}
static inline uint32_t uptime() {
    return syscall0(5);
}

// ── I/O syscalls ─────────────────────────────────────────────────────────────

static inline uint32_t write(const char *str, uint32_t len, uint32_t fd) {
    return syscall3(1, (uint32_t)str, len, fd);
}
static inline uint32_t read_stdin(char *buf, uint32_t len) {
    return syscall2(2, (uint32_t)buf, len);
}

// ── File I/O syscalls ────────────────────────────────────────────────────────

// Open a file by path. Returns fd (>=3) on success, -1 on failure.
static inline int open(const char *path) {
    return (int)syscall1(6, (uint32_t)path);
}

// Close an open file descriptor.
static inline int close(int fd) {
    return (int)syscall1(7, (uint32_t)fd);
}

// Read up to len bytes from fd into buf. Returns bytes read, 0 at EOF, -1 on error.
static inline int read_file(int fd, void *buf, uint32_t len) {
    return (int)syscall3(10, (uint32_t)fd, (uint32_t)buf, len);
}

// Seek fd to absolute offset. Returns 0 on success, -1 on error.
static inline int seek(int fd, uint32_t offset) {
    return (int)syscall2(11, (uint32_t)fd, offset);
}

// Return current position in fd.
static inline uint32_t tell(int fd) {
    return syscall1(12, (uint32_t)fd);
}

// Return total size of file open on fd.
static inline uint32_t fsize(int fd) {
    return syscall1(13, (uint32_t)fd);
}

// ── RamFS management ─────────────────────────────────────────────────────────

static inline uint32_t create(const char *name) {
    return syscall1(8, (uint32_t)name);
}
static inline uint32_t delete(const char *name) {
    return syscall1(9, (uint32_t)name);
}

// ── String / output helpers ──────────────────────────────────────────────────

static inline uint32_t strlen(const char *s) {
    uint32_t n = 0; while (s[n]) n++; return n;
}
static inline void puts(const char *s) {
    write(s, strlen(s), STDOUT);
}
static inline void putchar(char c) {
    write(&c, 1, STDOUT);
}
static inline void print_num(uint32_t n) {
    char buf[12]; int i = 11; buf[i] = '\0';
    if (n == 0) { putchar('0'); return; }
    while (n && i > 0) { buf[--i] = '0' + (n % 10); n /= 10; }
    puts(&buf[i]);
}
static inline void print_hex(uint32_t n) {
    char hex[9]; hex[8] = '\0';
    for (int i = 7; i >= 0; i--) {
        int nibble = n & 0xF;
        hex[i] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        n >>= 4;
    }
    puts(hex);
}

