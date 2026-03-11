#pragma once
#include <stdint.h>

#define COM1 0x3F8

void serial_init();
void serial_putchar(char c);
void serial_puts(const char *str);
void serial_print_num(uint32_t n);
void serial_print_hex(uint32_t n);

