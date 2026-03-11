#include "../include/vga.h"
#include "../include/io.h"
#include <stdint.h>

static int     cursor_col   = 0;
static int     cursor_row   = 0;
static uint8_t current_color;

static void update_hw_cursor() {
    uint16_t pos = cursor_row * VGA_WIDTH + cursor_col;
    outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void scroll() {
    for (int r = 1; r < VGA_HEIGHT; r++)
        for (int c = 0; c < VGA_WIDTH; c++)
            VGA_MEM[(r-1)*VGA_WIDTH+c] = VGA_MEM[r*VGA_WIDTH+c];
    for (int c = 0; c < VGA_WIDTH; c++)
        VGA_MEM[(VGA_HEIGHT-1)*VGA_WIDTH+c] =
            (uint16_t)' ' | ((uint16_t)current_color << 8);
    cursor_row = VGA_HEIGHT - 1;
}

void vga_init(uint8_t color) {
    current_color = color;
    cursor_col = 0;
    cursor_row = 0;
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_MEM[i] = (uint16_t)' ' | ((uint16_t)color << 8);
    update_hw_cursor();
}

void vga_set_color(uint8_t color) {
    current_color = color;
}

void vga_putchar(char c) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\b') {
        // Move back one, wrapping up a line if needed
        if (cursor_col > 0) {
            cursor_col--;
        } else if (cursor_row > 0) {
            cursor_row--;
            cursor_col = VGA_WIDTH - 1;
        }
        // Erase the character at new position
        VGA_MEM[cursor_row * VGA_WIDTH + cursor_col] =
            (uint16_t)' ' | ((uint16_t)current_color << 8);
    } else if (c == '\t') {
        cursor_col = (cursor_col + 8) & ~7;
    } else {
        VGA_MEM[cursor_row * VGA_WIDTH + cursor_col] =
            (uint16_t)c | ((uint16_t)current_color << 8);
        cursor_col++;
    }

    if (cursor_col >= VGA_WIDTH) {
        cursor_col = 0;
        cursor_row++;
    }
    if (cursor_row >= VGA_HEIGHT) {
        scroll();
    }

    update_hw_cursor();
}

void vga_puts(const char *str) {
    while (*str) vga_putchar(*str++);
}

void vga_clear() {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_MEM[i] = (uint16_t)' ' | ((uint16_t)current_color << 8);
    cursor_col = 0;
    cursor_row = 0;
    update_hw_cursor();
}

