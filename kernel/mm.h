#pragma once
#include <stdint.h>
#include <stddef.h>

void  mm_init(uint32_t mem_end);
void *kmalloc(size_t size);
void *kmalloc_aligned(size_t size);
void  kfree(void *ptr);
void  mm_print_stats();

