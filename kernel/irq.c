#include "irq.h"
#include "idt.h"
#include "../include/io.h"
#include <stdint.h>

extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();

static irq_handler_t irq_handlers[16] = {0};

static void pic_remap() {
    // Start init
    outb(0x20, 0x11);
    outb(0xA0, 0x11);

    // Vector offsets
    outb(0x21, 0x20);   // IRQ0-7  -> IDT 32
    outb(0xA1, 0x28);   // IRQ8-15 -> IDT 40

    // Cascade
    outb(0x21, 0x04);
    outb(0xA1, 0x02);

    // 8086 mode
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    // Unmask all IRQs
    outb(0x21, 0x00);
    outb(0xA1, 0x00);
}

void irq_init() {
    pic_remap();

    idt_set_gate(32, (uint32_t)irq0,  0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1,  0x08, 0x8E);
    idt_set_gate(34, (uint32_t)irq2,  0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3,  0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4,  0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5,  0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6,  0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7,  0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8,  0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9,  0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);
}

void irq_install_handler(int irq, irq_handler_t handler) {
    if (irq >= 0 && irq < 16)
        irq_handlers[irq] = handler;
}

void irq_handler(struct regs *r) {
    int irq = r->int_no - 32;

    // Send EOI before calling handler so we don't miss interrupts
    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);

    if (irq >= 0 && irq < 16 && irq_handlers[irq]) {
        irq_handlers[irq](r);
    }
}

