#include "irq.h"
#include "idt.h"
#include "../include/io.h"
#include <stdint.h>

#define IDT_ENTRIES 256

struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   ip;

extern void idt_flush(uint32_t);

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low  = (base & 0xFFFF);
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].selector  = sel;
    idt[num].zero      = 0;
    idt[num].flags     = flags;
}

void idt_init() {
    ip.limit = (sizeof(struct idt_entry) * IDT_ENTRIES) - 1;
    ip.base  = (uint32_t)&idt;

    // Zero out the entire IDT
    uint8_t *ptr = (uint8_t*)&idt;
    for (int i = 0; i < (int)sizeof(idt); i++) ptr[i] = 0;

    idt_flush((uint32_t)&ip);
}

