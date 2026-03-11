#pragma once
#include <stdint.h>

#define PAGE_SIZE       0x1000
#define PAGE_PRESENT    0x1
#define PAGE_WRITABLE   0x2
#define PAGE_USER       0x4

typedef struct {
    uint32_t entries[1024];
} page_table_t;

typedef struct {
    uint32_t entries[1024];
} page_directory_t;

void            paging_init();
void            paging_map(uint32_t virt, uint32_t phys, uint32_t flags);
uint32_t        paging_get_physical(uint32_t virt);
void            paging_print_info();

page_directory_t *paging_create_user_dir();
void              paging_destroy_user_dir(page_directory_t *dir);
void              paging_switch_dir(page_directory_t *dir);
void              paging_map_in_dir(page_directory_t *dir, uint32_t virt,
                                    uint32_t phys, uint32_t flags);
page_directory_t *paging_get_kernel_dir();

