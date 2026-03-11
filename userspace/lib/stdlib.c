#include "stdlib.h"

// ── Heap allocator ────────────────────────────────────────────────────────────
// Each block has a header immediately before the user data:
//   uint32_t size  — usable bytes (not including header)
//   uint32_t free  — 1=free, 0=in use
//   block_t *next  — next block in linked list

#define HDR_SIZE    (sizeof(block_t))
#define HEAP_ALIGN  8

typedef struct block {
    uint32_t      size;
    uint32_t      free;
    struct block *next;
} block_t;

static block_t *heap_list = 0;  // single definition — no extern confusion

static size_t align_up(size_t n) {
    return (n + HEAP_ALIGN - 1) & ~(HEAP_ALIGN - 1);
}

void *malloc(size_t size) {
    if (size == 0) return 0;
    size = align_up(size);

    // Walk list for a free block that fits
    block_t *b = heap_list;
    while (b) {
        if (b->free && b->size >= size) {
            b->free = 0;
            return (void*)((uint8_t*)b + HDR_SIZE);
        }
        b = b->next;
    }

    // No fit — grow heap via sbrk
    block_t *newb = (block_t*)sbrk((int)(HDR_SIZE + size));
    if ((int32_t)(uintptr_t)newb == -1) return 0;

    newb->size = size;
    newb->free = 0;
    newb->next = 0;

    // Append to list
    if (!heap_list) {
        heap_list = newb;
    } else {
        block_t *tail = heap_list;
        while (tail->next) tail = tail->next;
        tail->next = newb;
    }

    return (void*)((uint8_t*)newb + HDR_SIZE);
}

void free(void *ptr) {
    if (!ptr) return;
    block_t *b = (block_t*)((uint8_t*)ptr - HDR_SIZE);
    b->free = 1;

    // Coalesce adjacent free blocks
    block_t *cur = heap_list;
    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            cur->size += HDR_SIZE + cur->next->size;
            cur->next  = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *ptr = malloc(total);
    if (ptr) {
        uint8_t *p = (uint8_t*)ptr;
        for (size_t i = 0; i < total; i++) p[i] = 0;
    }
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr)  return malloc(size);
    if (!size) { free(ptr); return 0; }

    block_t *b = (block_t*)((uint8_t*)ptr - HDR_SIZE);
    if (b->size >= size) return ptr;  // already fits

    void *newptr = malloc(size);
    if (!newptr) return 0;

    uint8_t *src = (uint8_t*)ptr;
    uint8_t *dst = (uint8_t*)newptr;
    for (size_t i = 0; i < b->size; i++) dst[i] = src[i];
    free(ptr);
    return newptr;
}

