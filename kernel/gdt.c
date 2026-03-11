#include "gdt.h"
#include "serial.h"
#include <stdint.h>

#define GDT_ENTRIES 6

static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_ptr_t   gdt_ptr;
static tss_t       tss;

extern void gdt_flush(uint32_t);
extern void tss_flush();

static void gdt_set_gate(int num, uint32_t base, uint32_t limit,
                         uint8_t access, uint8_t gran) {
    gdt[num].base_low    = (base & 0xFFFF);
    gdt[num].base_mid    = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;
    gdt[num].limit_low   = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[num].access      = access;
}

static void tss_init(uint32_t kernel_stack) {
    uint32_t base  = (uint32_t)&tss;
    uint32_t limit = base + sizeof(tss_t);

    gdt_set_gate(5, base, limit, 0x89, 0x00);

    for (int i = 0; i < (int)sizeof(tss_t); i++)
        ((uint8_t*)&tss)[i] = 0;

    tss.ss0  = GDT_KERNEL_DATA;
    tss.esp0 = kernel_stack;
    tss.cs   = GDT_USER_CODE | 0x3;
    tss.ss   = GDT_USER_DATA | 0x3;
    tss.ds   = GDT_USER_DATA | 0x3;
    tss.es   = GDT_USER_DATA | 0x3;
    tss.fs   = GDT_USER_DATA | 0x3;
    tss.gs   = GDT_USER_DATA | 0x3;
}

void gdt_init() {
    gdt_ptr.limit = (sizeof(gdt_entry_t) * GDT_ENTRIES) - 1;
    gdt_ptr.base  = (uint32_t)&gdt;

    gdt_set_gate(0, 0, 0,          0x00, 0x00); // null
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // kernel code
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // kernel data
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // user code
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // user data
    tss_init(0x200000);

    gdt_flush((uint32_t)&gdt_ptr);
    tss_flush();

    serial_puts("[GDT] Ring 0/3 segments + TSS installed\n");
}

void tss_set_kernel_stack(uint32_t stack) {
    tss.esp0 = stack;
}

