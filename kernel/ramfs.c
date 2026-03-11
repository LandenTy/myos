#include "ramfs.h"
#include "../include/vga.h"
#include "../include/string.h"
#include <stdint.h>

static ramfs_file_t files[RAMFS_MAX_FILES];
static int          file_count = 0;

static void print_num(uint32_t n) {
    char buf[12]; int i = 11; buf[i] = '\0';
    if (n == 0) { vga_putchar('0'); return; }
    while (n && i > 0) { buf[--i] = '0' + (n % 10); n /= 10; }
    vga_puts(&buf[i]);
}

void ramfs_init() {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        files[i].used = 0;
        files[i].size = 0;
        files[i].name[0] = '\0';
    }
    file_count = 0;
}

int ramfs_exists(const char *name) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++)
        if (files[i].used && kstrcmp(files[i].name, name) == 0)
            return 1;
    return 0;
}

ramfs_file_t *ramfs_get(const char *name) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++)
        if (files[i].used && kstrcmp(files[i].name, name) == 0)
            return &files[i];
    return NULL;
}

int ramfs_create(const char *name) {
    if (ramfs_exists(name)) {
        vga_set_color(VGA_COLOR(VGA_LRED, VGA_BLACK));
        vga_puts(" Error: file already exists\n");
        vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
        return -1;
    }
    if (file_count >= RAMFS_MAX_FILES) {
        vga_set_color(VGA_COLOR(VGA_LRED, VGA_BLACK));
        vga_puts(" Error: filesystem full\n");
        vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
        return -1;
    }

    // Check name length
    size_t len = kstrlen(name);
    if (len == 0 || len >= RAMFS_MAX_FILENAME) {
        vga_set_color(VGA_COLOR(VGA_LRED, VGA_BLACK));
        vga_puts(" Error: invalid filename\n");
        vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
        return -1;
    }

    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!files[i].used) {
            files[i].used = 1;
            files[i].size = 0;
            // Copy name manually
            for (int j = 0; j < RAMFS_MAX_FILENAME; j++)
                files[i].name[j] = name[j];
            file_count++;
            vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
            vga_puts(" Created: ");
            vga_puts(name);
            vga_putchar('\n');
            vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
            return 0;
        }
    }
    return -1;
}

int ramfs_write(const char *name, const char *data, uint32_t size) {
    ramfs_file_t *f = ramfs_get(name);
    if (!f) {
        vga_set_color(VGA_COLOR(VGA_LRED, VGA_BLACK));
        vga_puts(" Error: file not found\n");
        vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
        return -1;
    }
    if (size > RAMFS_MAX_FILESIZE) size = RAMFS_MAX_FILESIZE;
    for (uint32_t i = 0; i < size; i++)
        f->data[i] = (uint8_t)data[i];
    f->size = size;
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" Wrote ");
    print_num(size);
    vga_puts(" bytes to ");
    vga_puts(name);
    vga_putchar('\n');
    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    return 0;
}

int ramfs_append(const char *name, const char *data, uint32_t size) {
    ramfs_file_t *f = ramfs_get(name);
    if (!f) {
        vga_set_color(VGA_COLOR(VGA_LRED, VGA_BLACK));
        vga_puts(" Error: file not found\n");
        vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
        return -1;
    }
    uint32_t space = RAMFS_MAX_FILESIZE - f->size;
    if (size > space) size = space;
    for (uint32_t i = 0; i < size; i++)
        f->data[f->size + i] = (uint8_t)data[i];
    f->size += size;
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" Appended ");
    print_num(size);
    vga_puts(" bytes to ");
    vga_puts(name);
    vga_putchar('\n');
    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    return 0;
}

int ramfs_read(const char *name) {
    ramfs_file_t *f = ramfs_get(name);
    if (!f) {
        vga_set_color(VGA_COLOR(VGA_LRED, VGA_BLACK));
        vga_puts(" Error: file not found\n");
        vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
        return -1;
    }
    if (f->size == 0) {
        vga_set_color(VGA_COLOR(VGA_LGRAY, VGA_BLACK));
        vga_puts(" (empty file)\n");
        vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
        return 0;
    }
    vga_set_color(VGA_COLOR(VGA_LCYAN, VGA_BLACK));
    for (uint32_t i = 0; i < f->size; i++)
        vga_putchar((char)f->data[i]);
    if (f->data[f->size - 1] != '\n')
        vga_putchar('\n');
    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    return 0;
}

int ramfs_delete(const char *name) {
    ramfs_file_t *f = ramfs_get(name);
    if (!f) {
        vga_set_color(VGA_COLOR(VGA_LRED, VGA_BLACK));
        vga_puts(" Error: file not found\n");
        vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
        return -1;
    }
    f->used = 0;
    f->size = 0;
    f->name[0] = '\0';
    file_count--;
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" Deleted: ");
    vga_puts(name);
    vga_putchar('\n');
    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    return 0;
}

void ramfs_ls() {
    if (file_count == 0) {
        vga_set_color(VGA_COLOR(VGA_LGRAY, VGA_BLACK));
        vga_puts(" (no files)\n");
        vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
        return;
    }

    vga_set_color(VGA_COLOR(VGA_YELLOW, VGA_BLACK));
    vga_puts(" NAME                             SIZE\n");
    vga_puts(" -------------------------------- --------\n");
    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));

    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!files[i].used) continue;

        vga_set_color(VGA_COLOR(VGA_LCYAN, VGA_BLACK));
        vga_puts(" ");
        vga_puts(files[i].name);

        // Pad to column 33
        int len = (int)kstrlen(files[i].name);
        for (int p = len; p < 33; p++) vga_putchar(' ');

        vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
        print_num(files[i].size);
        vga_puts(" bytes\n");
    }

    vga_set_color(VGA_COLOR(VGA_YELLOW, VGA_BLACK));
    vga_puts(" ");
    print_num((uint32_t)file_count);
    vga_puts(" file(s)\n");
    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
}

