#include "timer.h"
#include "irq.h"
#include "../include/io.h"
#include <stdint.h>

static volatile uint32_t ticks = 0;
static          uint32_t hz    = 0;

static void timer_irq_handler(struct regs *r) {
    (void)r;
    ticks++;
    // No scheduler call here — sleep is handled inline in syscall handler
}

void timer_init(uint32_t frequency) {
    hz = frequency;
    irq_install_handler(0, timer_irq_handler);
    uint32_t divisor = 1193180 / frequency;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

uint32_t timer_get_ticks() {
    return ticks;
}

void timer_sleep(uint32_t ms) {
    uint32_t target = ticks + (hz * ms / 1000);
    __asm__ volatile ("sti");
    while (ticks < target) {
        __asm__ volatile ("hlt");
    }
}

