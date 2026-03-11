#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include "string.h"
#include "stdlib.h"

// ── syscall helpers ───────────────────────────────────────────────────────────
static inline uint32_t _sys1(uint32_t n, uint32_t a) {
    uint32_t r; __asm__ volatile("int $0x80":"=a"(r):"a"(n),"b"(a):"memory"); return r;
}
static inline uint32_t _sys2(uint32_t n, uint32_t a, uint32_t b) {
    uint32_t r; __asm__ volatile("int $0x80":"=a"(r):"a"(n),"b"(a),"c"(b):"memory"); return r;
}
static inline uint32_t _sys3(uint32_t n, uint32_t a, uint32_t b, uint32_t c) {
    uint32_t r; __asm__ volatile("int $0x80":"=a"(r):"a"(n),"b"(a),"c"(b),"d"(c):"memory"); return r;
}

// ── FILE type ─────────────────────────────────────────────────────────────────
typedef struct {
    int      fd;
    uint32_t size;
    int      eof;
    int      error;
} FILE;

#define stdin  ((FILE*)0)
#define stdout ((FILE*)1)
#define stderr ((FILE*)2)

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

// ── Low-level output ──────────────────────────────────────────────────────────
static inline int putchar(int c) {
    char ch = (char)c;
    _sys3(1, (uint32_t)&ch, 1, 1);
    return c;
}

static inline int puts(const char *s) {
    if (!s) s = "(null)";
    uint32_t len = strlen(s);
    _sys3(1, (uint32_t)s, len, 1);
    putchar('\n');
    return (int)len + 1;
}

static inline int fputs(const char *s, FILE *stream) {
    if (!s) s = "(null)";
    uint32_t len = strlen(s);
    int fd = (stream == stderr) ? 2 : 1;
    _sys3(1, (uint32_t)s, len, (uint32_t)fd);
    return (int)len;
}

// ── Core formatting engine (writes to buffer OR screen) ──────────────────────
// buf=NULL means write directly to screen via putchar.
// Returns number of characters written (not including NUL).
static inline int _vformat(char *buf, int buf_size, const char *fmt, va_list args) {
    int pos = 0;

#define EMIT(c) do { \
    if (buf) { if (pos < buf_size - 1) buf[pos] = (c); } \
    else putchar((c)); \
    pos++; \
} while(0)

    while (*fmt) {
        if (*fmt != '%') { EMIT(*fmt++); continue; }
        fmt++;

        // Flags
        int left = 0, zero_pad = 0, plus = 0;
        int done = 0;
        while (!done) {
            switch (*fmt) {
                case '-': left     = 1; fmt++; break;
                case '0': zero_pad = 1; fmt++; break;
                case '+': plus     = 1; fmt++; break;
                default:  done = 1; break;
            }
        }
        if (left) zero_pad = 0;

        // Width
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9')
            width = width * 10 + (*fmt++ - '0');

        // Precision
        int prec = -1;
        if (*fmt == '.') {
            fmt++; prec = 0;
            while (*fmt >= '0' && *fmt <= '9')
                prec = prec * 10 + (*fmt++ - '0');
        }

        char spec = *fmt++;
        char tmp[32];

        switch (spec) {
            case 'c': {
                char c = (char)va_arg(args, int);
                if (!left) for (int i = 1; i < width; i++) EMIT(' ');
                EMIT(c);
                if (left)  for (int i = 1; i < width; i++) EMIT(' ');
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char*);
                if (!s) s = "(null)";
                int len = (int)strlen(s);
                if (prec >= 0 && len > prec) len = prec;
                if (!left) for (int i = len; i < width; i++) EMIT(' ');
                for (int i = 0; i < len; i++) EMIT(s[i]);
                if (left)  for (int i = len; i < width; i++) EMIT(' ');
                break;
            }
            case 'd': case 'i': {
                int32_t n = va_arg(args, int32_t);
                int neg = (n < 0);
                uint32_t un = neg ? (uint32_t)(-n) : (uint32_t)n;
                int i = 31; tmp[i] = '\0';
                if (un == 0) tmp[--i] = '0';
                else while (un) { tmp[--i] = '0' + un % 10; un /= 10; }
                if (neg) tmp[--i] = '-';
                else if (plus) tmp[--i] = '+';
                const char *s = &tmp[i];
                int len = (int)strlen(s);
                char pad = (zero_pad && !left) ? '0' : ' ';
                if (!left) for (int j = len; j < width; j++) EMIT(pad);
                for (int j = 0; s[j]; j++) EMIT(s[j]);
                if (left)  for (int j = len; j < width; j++) EMIT(' ');
                break;
            }
            case 'u': {
                uint32_t n = va_arg(args, uint32_t);
                int i = 31; tmp[i] = '\0';
                if (n == 0) tmp[--i] = '0';
                else while (n) { tmp[--i] = '0' + n % 10; n /= 10; }
                const char *s = &tmp[i];
                int len = (int)strlen(s);
                char pad = (zero_pad && !left) ? '0' : ' ';
                if (!left) for (int j = len; j < width; j++) EMIT(pad);
                for (int j = 0; s[j]; j++) EMIT(s[j]);
                if (left)  for (int j = len; j < width; j++) EMIT(' ');
                break;
            }
            case 'x': case 'X': {
                uint32_t n = va_arg(args, uint32_t);
                int upper = (spec == 'X');
                int i = 31; tmp[i] = '\0';
                if (n == 0) tmp[--i] = '0';
                else while (n) {
                    int d = n % 16;
                    tmp[--i] = d < 10 ? '0'+d : (upper?'A':'a')+d-10;
                    n /= 16;
                }
                const char *s = &tmp[i];
                int len = (int)strlen(s);
                char pad = (zero_pad && !left) ? '0' : ' ';
                if (!left) for (int j = len; j < width; j++) EMIT(pad);
                for (int j = 0; s[j]; j++) EMIT(s[j]);
                if (left)  for (int j = len; j < width; j++) EMIT(' ');
                break;
            }
            case 'p': {
                uint32_t n = va_arg(args, uint32_t);
                EMIT('0'); EMIT('x');
                int i = 31; tmp[i] = '\0';
                if (n == 0) tmp[--i] = '0';
                else while (n) { int d=n%16; tmp[--i]=d<10?'0'+d:'a'+d-10; n/=16; }
                for (char *s = &tmp[i]; *s; s++) EMIT(*s);
                break;
            }
            case '%': EMIT('%'); break;
            default:  EMIT('?'); break;
        }
    }

#undef EMIT
    if (buf && pos < buf_size) buf[pos] = '\0';
    return pos;
}

// ── printf / sprintf / snprintf ───────────────────────────────────────────────
static inline int printf(const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    int r = _vformat(0, 0x7FFFFFFF, fmt, args);
    va_end(args);
    return r;
}

static inline int sprintf(char *buf, const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    int r = _vformat(buf, 0x7FFFFFFF, fmt, args);
    va_end(args);
    return r;
}

static inline int snprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list args; va_start(args, fmt);
    int r = _vformat(buf, (int)size, fmt, args);
    va_end(args);
    return r;
}

static inline int vprintf(const char *fmt, va_list args) {
    return _vformat(0, 0x7FFFFFFF, fmt, args);
}

static inline int vsprintf(char *buf, const char *fmt, va_list args) {
    return _vformat(buf, 0x7FFFFFFF, fmt, args);
}

static inline int vsnprintf(char *buf, size_t size, const char *fmt, va_list args) {
    return _vformat(buf, (int)size, fmt, args);
}

// ── getchar / gets ────────────────────────────────────────────────────────────
static inline int getchar(void) {
    char c;
    _sys3(2, (uint32_t)&c, 1, 0);
    return (int)(unsigned char)c;
}

static inline char *gets(char *buf) {
    int i = 0;
    while (1) {
        int c = getchar();
        if (c == '\n' || c == '\r') break;
        buf[i++] = (char)c;
    }
    buf[i] = '\0';
    return buf;
}

static inline char *fgets_stdin(char *buf, int size) {
    int i = 0;
    while (i < size - 1) {
        int c = getchar();
        if (c == '\n' || c == '\r') break;
        buf[i++] = (char)c;
    }
    buf[i] = '\0';
    return buf;
}

// ── scanf (basic) ─────────────────────────────────────────────────────────────
static inline int scanf(const char *fmt, ...) {
    // Read a line then parse it
    char line[256];
    fgets_stdin(line, sizeof(line));

    va_list args; va_start(args, fmt);
    int matched = 0;
    const char *p = line;

    while (*fmt && *p) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt++) {
                case 'd': case 'i': {
                    int *out = va_arg(args, int*);
                    int neg = 0, val = 0;
                    while (*p == ' ') p++;
                    if (*p == '-') { neg = 1; p++; }
                    while (*p >= '0' && *p <= '9') { val = val*10 + (*p++ - '0'); }
                    *out = neg ? -val : val;
                    matched++;
                    break;
                }
                case 's': {
                    char *out = va_arg(args, char*);
                    while (*p == ' ') p++;
                    while (*p && *p != ' ' && *p != '\n') *out++ = *p++;
                    *out = '\0';
                    matched++;
                    break;
                }
                case 'c': {
                    char *out = va_arg(args, char*);
                    *out = *p++;
                    matched++;
                    break;
                }
            }
        } else {
            if (*fmt++ != *p++) break;
        }
    }
    va_end(args);
    return matched;
}

// ── File I/O ──────────────────────────────────────────────────────────────────
static inline FILE *fopen(const char *path, const char *mode) {
    // mode: "r"=read, "w"=write/create, "a"=append
    int write_mode = (mode && (mode[0] == 'w' || mode[0] == 'a'));
    int fd;
    if (write_mode) {
        fd = (int)_sys2(8, (uint32_t)path, (uint32_t)mode[0]); // SYSCALL_CREATE
    } else {
        fd = (int)_sys1(6, (uint32_t)path); // SYSCALL_OPEN
    }
    if ((int32_t)fd < 0) return 0;

    FILE *f = (FILE*)malloc(sizeof(FILE));
    if (!f) { _sys1(7, (uint32_t)fd); return 0; }
    f->fd    = fd;
    f->size  = write_mode ? 0 : _sys1(13, (uint32_t)fd);
    f->eof   = 0;
    f->error = 0;
    return f;
}

static inline int fclose(FILE *f) {
    if (!f || f->fd < 0) return -1;
    _sys1(7, (uint32_t)f->fd);
    free(f);
    return 0;
}

static inline size_t fread(void *buf, size_t size, size_t nmemb, FILE *f) {
    if (!f || f->eof || f->error) return 0;
    uint32_t total = (uint32_t)(size * nmemb);
    int got = (int)_sys3(10, (uint32_t)f->fd, (uint32_t)buf, total);
    if (got <= 0) { f->eof = 1; return 0; }
    return (size_t)got / size;
}

static inline size_t fwrite(const void *buf, size_t size, size_t nmemb, FILE *f) {
    if (!f || f->error) return 0;
    uint32_t total = (uint32_t)(size * nmemb);
    int wrote = (int)_sys3(15, (uint32_t)f->fd, (uint32_t)buf, total); // SYSCALL_WRITE_FILE
    if (wrote < 0) { f->error = 1; return 0; }
    f->size += (uint32_t)wrote;
    return (size_t)wrote / size;
}

static inline int fseek(FILE *f, long offset, int whence) {
    if (!f) return -1;
    uint32_t new_pos;
    if      (whence == SEEK_SET) new_pos = (uint32_t)offset;
    else if (whence == SEEK_CUR) new_pos = _sys1(12, (uint32_t)f->fd) + (uint32_t)offset;
    else                         new_pos = f->size + (uint32_t)offset;
    int r = (int)_sys3(11, (uint32_t)f->fd, new_pos, 0);
    if (r == 0) f->eof = 0;
    return r;
}

static inline long ftell(FILE *f) {
    if (!f) return -1;
    return (long)_sys1(12, (uint32_t)f->fd);
}

static inline int feof(FILE *f)   { return f ? f->eof   : 1; }
static inline int ferror(FILE *f) { return f ? f->error : 1; }
static inline void clearerr(FILE *f) { if (f) { f->eof = 0; f->error = 0; } }

static inline char *fgets(char *buf, int size, FILE *f) {
    if (!f || f->eof || size <= 0) return 0;
    int i = 0;
    while (i < size - 1) {
        char c;
        int got = (int)_sys3(10, (uint32_t)f->fd, (uint32_t)&c, 1);
        if (got <= 0) { f->eof = 1; break; }
        buf[i++] = c;
        if (c == '\n') break;
    }
    if (i == 0) return 0;
    buf[i] = '\0';
    return buf;
}

static inline int fprintf(FILE *stream, const char *fmt, ...) {
    // Build into a buffer then write to the fd
    char buf[512];
    va_list args; va_start(args, fmt);
    int len = _vformat(buf, sizeof(buf), fmt, args);
    va_end(args);
    int fd = (stream == stderr) ? 2 : (stream == stdout) ? 1 :
             (stream ? stream->fd : 1);
    _sys3(1, (uint32_t)buf, (uint32_t)len, (uint32_t)fd);
    return len;
}
