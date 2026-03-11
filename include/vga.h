#pragma once
#include <stdint.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEM    ((uint16_t*)0xB8000)

#define VGA_COLOR(fg, bg) ((uint8_t)((bg << 4) | fg))

enum vga_color {
    VGA_BLACK    = 0,
    VGA_BLUE     = 1,
    VGA_GREEN    = 2,
    VGA_CYAN     = 3,
    VGA_RED      = 4,
    VGA_MAGENTA  = 5,
    VGA_BROWN    = 6,
    VGA_LGRAY    = 7,
    VGA_DGRAY    = 8,
    VGA_LBLUE    = 9,
    VGA_LGREEN   = 10,
    VGA_LCYAN    = 11,
    VGA_LRED     = 12,
    VGA_LMAGENTA = 13,
    VGA_YELLOW   = 14,
    VGA_WHITE    = 15,
};

void vga_init(uint8_t color);
void vga_set_color(uint8_t color);
void vga_putchar(char c);
void vga_puts(const char *str);
void vga_clear();

