#include "../include/vga.h"
#include "gdt.h"
#include "idt.h"
#include "irq.h"
#include "keyboard.h"
#include "timer.h"
#include "mm.h"
#include "paging.h"
#include "ramfs.h"
#include "scheduler.h"
#include "serial.h"
#include "syscall.h"
#include "programs.h"
#include "shell.h"
#include "ata.h"
#include "ext2.h"

void kernel_main() {
    vga_init(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    serial_init();
    serial_puts("\n[Mars] Boot started\n");

    gdt_init();
    serial_puts("[Mars] GDT initialized\n");
    idt_init();
    serial_puts("[Mars] IDT initialized\n");
    irq_init();
    serial_puts("[Mars] IRQs initialized\n");
    keyboard_init();
    serial_puts("[Mars] Keyboard initialized\n");
    timer_init(1000);
    serial_puts("[Mars] Timer initialized\n");
    __asm__ volatile ("sti");
    serial_puts("[Mars] Interrupts enabled\n");

    mm_init(0);
    serial_puts("[Mars] Memory manager initialized\n");
    paging_init();
    serial_puts("[Mars] Paging enabled\n");
    ramfs_init();
    serial_puts("[Mars] RamFS initialized\n");
    syscall_init();
    serial_puts("[Mars] Syscall interface initialized (int 0x80)\n");
    programs_init();
    serial_puts("[Mars] Programs loaded\n");

    ata_init();
    serial_puts("[Mars] ATA initialized\n");
    ext2_init();
    serial_puts("[Mars] ext2 initialized\n");

    scheduler_init();
    serial_puts("[Mars] Scheduler initialized\n");

    serial_puts("[Mars] Entering shell\n\n");
    shell_run();
}

