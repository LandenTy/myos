#pragma once
#include <stdint.h>

struct regs {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp;
    uint32_t ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags;
    uint32_t useresp, ss;
};

typedef void (*irq_handler_t)(struct regs *r);

void irq_init();
void irq_install_handler(int irq, irq_handler_t handler);
void irq_uninstall_handler(int irq);

