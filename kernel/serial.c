#include "serial.h"
#include "../include/io.h"
#include <stdint.h>

void serial_init() {
    outb(COM1 + 1, 0x00);   // Disable interrupts
    outb(COM1 + 3, 0x80);   // Enable DLAB to set baud rate
    outb(COM1 + 0, 0x03);   // Baud rate low byte  (38400 baud)
    outb(COM1 + 1, 0x00);   // Baud rate high byte
    outb(COM1 + 3, 0x03);   // 8 bits, no parity, one stop bit
    outb(COM1 + 2, 0xC7);   // Enable FIFO, clear, 14-byte threshold
    outb(COM1 + 4, 0x0B);   // IRQs enabled, RTS/DSR set
}

static int serial_transmit_empty() {
    return inb(COM1 + 5) & 0x20;
}

void serial_putchar(char c) {
    while (!serial_transmit_empty());
    if (c == '\n') {
        outb(COM1, '\r');
        while (!serial_transmit_empty());
    }
    outb(COM1, (uint8_t)c);
}

void serial_puts(const char *str) {
    while (*str) serial_putchar(*str++);
}

void serial_print_num(uint32_t n) {
    char buf[12]; int i = 11; buf[i] = '\0';
    if (n == 0) { serial_putchar('0'); return; }
    while (n && i > 0) { buf[--i] = '0' + (n % 10); n /= 10; }
    serial_puts(&buf[i]);
}

void serial_print_hex(uint32_t n) {
    serial_puts("0x");
    char hex[9]; hex[8] = '\0';
    for (int i = 7; i >= 0; i--) {
        int nibble = n & 0xF;
        hex[i] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        n >>= 4;
    }
    serial_puts(hex);
}

