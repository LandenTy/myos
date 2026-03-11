#include "mm.h"
#include "../include/vga.h"
#include <stdint.h>
#include <stddef.h>

extern uint32_t kernel_end;

#define HEAP_SIZE    (8 * 1024 * 1024)
#define MAGIC_FREE   0xDEADBEEF
#define MAGIC_USED   0xCAFEBABE
#define MIN_SPLIT    32

typedef struct block_header {
    uint32_t            magic;
    size_t              size;
    int                 free;
    struct block_header *next;
    struct block_header *prev;
} block_header_t;

static block_header_t *heap_start = NULL;
static uint32_t        heap_base  = 0;

static void print_num(uint32_t n) {
    char buf[12];
    int  i = 11;
    buf[i] = '\0';
    if (n == 0) { vga_putchar('0'); return; }
    while (n && i > 0) {
        buf[--i] = '0' + (n % 10);
        n /= 10;
    }
    vga_puts(&buf[i]);
}

void mm_init(uint32_t mem_end) {
    (void)mem_end;
    heap_base = ((uint32_t)&kernel_end + 0xFFF) & ~0xFFF;
    heap_start = (block_header_t*)heap_base;
    heap_start->magic = MAGIC_FREE;
    heap_start->size  = HEAP_SIZE - sizeof(block_header_t);
    heap_start->free  = 1;
    heap_start->next  = NULL;
    heap_start->prev  = NULL;
}

static block_header_t *find_free(size_t size) {
    block_header_t *cur = heap_start;
    while (cur) {
        if (cur->free && cur->magic == MAGIC_FREE && cur->size >= size)
            return cur;
        cur = cur->next;
    }
    return NULL;
}

static void split(block_header_t *block, size_t size) {
    if (block->size < size + sizeof(block_header_t) + MIN_SPLIT)
        return;

    block_header_t *new_block = (block_header_t*)((uint8_t*)block
                                + sizeof(block_header_t) + size);
    new_block->magic = MAGIC_FREE;
    new_block->size  = block->size - size - sizeof(block_header_t);
    new_block->free  = 1;
    new_block->next  = block->next;
    new_block->prev  = block;

    if (block->next)
        block->next->prev = new_block;

    block->next = new_block;
    block->size = size;
}

void *kmalloc(size_t size) {
    if (!size) return NULL;
    size = (size + 7) & ~7;
    block_header_t *block = find_free(size);
    if (!block) return NULL;
    split(block, size);
    block->free  = 0;
    block->magic = MAGIC_USED;
    return (void*)((uint8_t*)block + sizeof(block_header_t));
}

void *kmalloc_aligned(size_t size) {
    uint8_t *raw = (uint8_t*)kmalloc(size + 4096 + sizeof(uint32_t));
    if (!raw) return NULL;
    uint32_t aligned = ((uint32_t)raw + sizeof(uint32_t) + 4095) & ~4095;
    ((uint32_t*)aligned)[-1] = (uint32_t)raw;
    return (void*)aligned;
}

static void coalesce(block_header_t *block) {
    if (block->next && block->next->free) {
        block->size += sizeof(block_header_t) + block->next->size;
        block->next  = block->next->next;
        if (block->next)
            block->next->prev = block;
    }
    if (block->prev && block->prev->free) {
        block->prev->size += sizeof(block_header_t) + block->size;
        block->prev->next  = block->next;
        if (block->next)
            block->next->prev = block->prev;
    }
}

void kfree(void *ptr) {
    if (!ptr) return;
    block_header_t *block = (block_header_t*)((uint8_t*)ptr
                            - sizeof(block_header_t));
    if (block->magic != MAGIC_USED) return;
    block->free  = 1;
    block->magic = MAGIC_FREE;
    coalesce(block);
}

void mm_print_stats() {
    size_t free_bytes  = 0;
    size_t used_bytes  = 0;
    int    free_blocks = 0;
    int    used_blocks = 0;

    block_header_t *cur = heap_start;
    while (cur) {
        if (cur->free) { free_bytes  += cur->size; free_blocks++; }
        else           { used_bytes  += cur->size; used_blocks++; }
        cur = cur->next;
    }

    vga_set_color(VGA_COLOR(VGA_YELLOW, VGA_BLACK));
    vga_puts(" -- Memory Stats --\n");
    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));

    vga_puts(" Free  : "); print_num((uint32_t)free_bytes);
    vga_puts(" bytes in "); print_num(free_blocks); vga_puts(" blocks\n");

    vga_puts(" Used  : "); print_num((uint32_t)used_bytes);
    vga_puts(" bytes in "); print_num(used_blocks); vga_puts(" blocks\n");

    vga_puts(" Heap  @ 0x");
    uint32_t addr = heap_base;
    char hex[9]; hex[8] = '\0';
    for (int i = 7; i >= 0; i--) {
        int nibble = addr & 0xF;
        hex[i] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        addr >>= 4;
    }
    vga_puts(hex);
    vga_putchar('\n');
}

