#include "paging.h"
#include "mm.h"
#include "idt.h"
#include "irq.h"
#include "serial.h"
#include "../include/vga.h"
#include <stdint.h>

static page_directory_t *kernel_dir = NULL;

static page_table_t *alloc_page_table() {
    page_table_t *pt = (page_table_t*)kmalloc_aligned(sizeof(page_table_t));
    if (!pt) return NULL;
    for (int i = 0; i < 1024; i++) pt->entries[i] = 0;
    return pt;
}

static void page_fault_handler(struct regs *r) {
    uint32_t fault_addr;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_addr));
    uint32_t err = r->err_code;

    vga_set_color(VGA_COLOR(VGA_LRED, VGA_BLACK));
    vga_puts("\n *** PAGE FAULT ***\n");
    vga_puts(" Faulting address: 0x");

    char hex[9]; hex[8] = '\0';
    uint32_t a = fault_addr;
    for (int i = 7; i >= 0; i--) {
        int n = a & 0xF;
        hex[i] = n < 10 ? '0' + n : 'A' + n - 10;
        a >>= 4;
    }
    vga_puts(hex);
    vga_putchar('\n');

    vga_puts(err & 0x1 ? " Cause  : Protection violation\n"
                       : " Cause  : Non-present page\n");
    vga_puts(err & 0x2 ? " Access : Write\n"
                       : " Access : Read\n");
    vga_puts(err & 0x4 ? " Mode   : User\n"
                       : " Mode   : Kernel\n");

    serial_puts("[paging] PAGE FAULT at 0x");
    serial_print_hex(fault_addr);
    serial_puts(err & 0x4 ? " (user)\n" : " (kernel)\n");

    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts(" System halted.\n");
    __asm__ volatile ("cli; hlt");
}

void paging_map(uint32_t virt, uint32_t phys, uint32_t flags) {
    paging_map_in_dir(kernel_dir, virt, phys, flags);
}

void paging_map_in_dir(page_directory_t *dir, uint32_t virt,
                       uint32_t phys, uint32_t flags) {
    uint32_t pd_idx = virt >> 22;
    uint32_t pt_idx = (virt >> 12) & 0x3FF;

    page_table_t *pt;

    if (dir->entries[pd_idx] & PAGE_PRESENT) {
        pt = (page_table_t*)(dir->entries[pd_idx] & ~0xFFF);
    } else {
        pt = alloc_page_table();
        dir->entries[pd_idx] = (uint32_t)pt | PAGE_PRESENT
                               | PAGE_WRITABLE | PAGE_USER;
    }

    pt->entries[pt_idx] = (phys & ~0xFFF) | (flags & 0xFFF) | PAGE_PRESENT;
}

uint32_t paging_get_physical(uint32_t virt) {
    uint32_t pd_idx = virt >> 22;
    uint32_t pt_idx = (virt >> 12) & 0x3FF;

    if (!(kernel_dir->entries[pd_idx] & PAGE_PRESENT)) return 0;
    page_table_t *pt = (page_table_t*)(kernel_dir->entries[pd_idx] & ~0xFFF);
    if (!(pt->entries[pt_idx] & PAGE_PRESENT)) return 0;
    return (pt->entries[pt_idx] & ~0xFFF) | (virt & 0xFFF);
}

page_directory_t *paging_get_kernel_dir() {
    return kernel_dir;
}

page_directory_t *paging_create_user_dir() {
    page_directory_t *dir = (page_directory_t*)kmalloc_aligned(
                                sizeof(page_directory_t));
    if (!dir) return NULL;

    for (int i = 0; i < 1024; i++) dir->entries[i] = 0;

    // Copy kernel page directory entries into the new dir
    // so the kernel is accessible from userspace via syscalls
    for (int i = 0; i < 1024; i++) {
        if (kernel_dir->entries[i] & PAGE_PRESENT) {
            dir->entries[i] = kernel_dir->entries[i];
        }
    }

    return dir;
}

void paging_destroy_user_dir(page_directory_t *dir) {
    if (!dir || dir == kernel_dir) return;

    // Free page tables that aren't shared with kernel
    for (int i = 0; i < 1024; i++) {
        if (dir->entries[i] & PAGE_PRESENT) {
            if (!(kernel_dir->entries[i] & PAGE_PRESENT)) {
                page_table_t *pt = (page_table_t*)(dir->entries[i] & ~0xFFF);
                kfree(pt);
            }
        }
    }
    kfree(dir);
}

void paging_switch_dir(page_directory_t *dir) {
    __asm__ volatile (
        "mov %0, %%cr3\n"
        : : "r"(dir) : "memory"
    );
}

void paging_init() {
    idt_set_gate(14, (uint32_t)page_fault_handler, 0x08, 0x8E);

    kernel_dir = (page_directory_t*)kmalloc_aligned(sizeof(page_directory_t));
    for (int i = 0; i < 1024; i++) kernel_dir->entries[i] = 0;

    for (uint32_t addr = 0; addr < 0x2000000; addr += PAGE_SIZE) {
        paging_map(addr, addr, PAGE_PRESENT | PAGE_WRITABLE);
    }

    __asm__ volatile (
        "mov %0, %%cr3\n"
        "mov %%cr0, %%eax\n"
        "or $0x80000000, %%eax\n"
        "mov %%eax, %%cr0\n"
        : : "r"(kernel_dir) : "eax"
    );
}

void paging_print_info() {
    vga_set_color(VGA_COLOR(VGA_YELLOW, VGA_BLACK));
    vga_puts(" -- Paging Info --\n");
    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));

    uint32_t mapped = 0;
    for (int i = 0; i < 1024; i++) {
        if (kernel_dir->entries[i] & PAGE_PRESENT) {
            page_table_t *pt = (page_table_t*)(kernel_dir->entries[i] & ~0xFFF);
            for (int j = 0; j < 1024; j++) {
                if (pt->entries[j] & PAGE_PRESENT) mapped++;
            }
        }
    }

    vga_puts(" Mapped pages : ");
    char buf[12]; int k = 11; buf[k] = '\0';
    uint32_t n = mapped;
    if (n == 0) { vga_putchar('0'); }
    else { while (n && k > 0) { buf[--k] = '0'+(n%10); n/=10; } vga_puts(&buf[k]); }
    vga_putchar('\n');

    vga_puts(" Mapped memory: ");
    char buf2[12]; k = 11; buf2[k] = '\0';
    n = mapped * 4;
    if (n == 0) { vga_putchar('0'); }
    else { while (n && k > 0) { buf2[--k] = '0'+(n%10); n/=10; } vga_puts(&buf2[k]); }
    vga_puts(" KB\n");

    uint32_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    vga_puts(" CR3 (page dir): 0x");
    char hex[9]; hex[8] = '\0';
    for (int i = 7; i >= 0; i--) {
        int nibble = cr3 & 0xF;
        hex[i] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        cr3 >>= 4;
    }
    vga_puts(hex);
    vga_putchar('\n');
}

