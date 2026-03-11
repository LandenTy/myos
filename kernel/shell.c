#include "../include/vga.h"
#include "../include/string.h"
#include "keyboard.h"
#include "timer.h"
#include "mm.h"
#include "paging.h"
#include "ramfs.h"
#include "scheduler.h"
#include "serial.h"
#include "syscall.h"
#include "syscall_user.h"
#include "elf.h"
#include "ext2.h"
#include <stdint.h>

#define CMD_BUF_SIZE 256
#define KLOG_SIZE    512

static char cmd_buf[CMD_BUF_SIZE];
static int  cmd_len  = 0;
static char klog_buf[KLOG_SIZE];
static int  klog_len = 0;

void klog(const char *msg) {
    serial_puts(msg);
    for (int i = 0; msg[i] && klog_len < KLOG_SIZE - 1; i++)
        klog_buf[klog_len++] = msg[i];
}

static void print_num(uint32_t n) {
    char buf[12]; int i = 11; buf[i] = '\0';
    if (n == 0) { vga_putchar('0'); return; }
    while (n && i > 0) { buf[--i] = '0' + (n % 10); n /= 10; }
    vga_puts(&buf[i]);
}

static void print_hex(uint32_t n) {
    char hex[9]; hex[8] = '\0';
    for (int i = 7; i >= 0; i--) {
        int nibble = n & 0xF;
        hex[i] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        n >>= 4;
    }
    vga_puts(hex);
}

static void print_banner() {
    vga_set_color(VGA_COLOR(VGA_YELLOW, VGA_BLACK));
    vga_puts("     /\\__\\         /\\  \\         /\\  \\         /\\  \\    \n");
    vga_puts("    /::|  |       /::\\  \\       /::\\  \\       /::\\  \\   \n");
    vga_puts("   /:|:|  |      /:/\\:\\  \\     /:/\\:\\  \\     /:/\\ \\  \\  \n");
    vga_puts("  /:/|:|__|__   /::\\~\\:\\  \\   /::\\~\\:\\  \\   _\\:\\~\\ \\  \\ \n");
    vga_puts(" /:/ |::::\\__\\ /:/\\:\\ \\:\\__\\ /:/\\:\\ \\:\\__\\ /\\ \\:\\ \\ \\__\\\n");
    vga_puts(" \\/__/~~/:/  / \\/__\\:\\/:/  / \\/_|::\\/:/  / \\:\\ \\:\\ \\/__/ \n");
    vga_puts("       /:/  /       \\::/  /     |:|::/  /   \\:\\ \\:\\__\\  \n");
    vga_puts("      /:/  /        /:/  /      |:|\\/__/     \\:\\/:/  /  \n");
    vga_puts("     /:/  /        /:/  /       |:|  |        \\::/  /   \n");
    vga_puts("     \\/__/         \\/__/         \\|__|         \\/__/    \n");
    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts("\n Welcome to Mars v0.8\n");
    vga_puts(" Type 'help' to see available commands.\n\n");
}

static void cmd_help() {
    vga_set_color(VGA_COLOR(VGA_YELLOW, VGA_BLACK));
    vga_puts("\n -- General --\n");
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" help              "); vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts("- This message\n");
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" clear             "); vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts("- Clear the screen\n");
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" hello             "); vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts("- Print a greeting\n");
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" about             "); vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts("- About this OS\n");
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" uptime            "); vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts("- System uptime\n");
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" sleep <ms>        "); vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts("- Sleep N milliseconds\n");
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" dmesg             "); vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts("- Show kernel log\n");
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" reboot            "); vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts("- Reboot\n");

    vga_set_color(VGA_COLOR(VGA_YELLOW, VGA_BLACK));
    vga_puts("\n -- Memory --\n");
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" mem               "); vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts("- Memory stats\n");
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" memtest           "); vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts("- kmalloc/kfree test\n");
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" paging            "); vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts("- Paging info\n");
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" maptest           "); vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts("- Address translation test\n");

    vga_set_color(VGA_COLOR(VGA_YELLOW, VGA_BLACK));
    vga_puts("\n -- Filesystem --\n");
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" ls                "); vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts("- List ramfs files\n");
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" lsext2 [path]     "); vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts("- List ext2 disk directory\n");
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" create <name>     "); vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts("- Create a ramfs file\n");
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" write <name> <text>   "); vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts("- Write text to file\n");
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" append <name> <text>  "); vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts("- Append text to file\n");
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" read <name>       "); vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts("- Read a ramfs file\n");
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" rm <name>         "); vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts("- Delete a ramfs file\n");

    vga_set_color(VGA_COLOR(VGA_YELLOW, VGA_BLACK));
    vga_puts("\n -- Processes --\n");
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" ps                "); vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts("- List all processes\n");
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" exec <name>       "); vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts("- Run ELF from ramfs\n");
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" execd <name>      "); vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts("- Run ELF from ext2 disk\n");

    vga_set_color(VGA_COLOR(VGA_YELLOW, VGA_BLACK));
    vga_puts("\n -- Syscalls --\n");
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" syscalltest       "); vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts("- Test the syscall interface\n");
    vga_putchar('\n');
}

static void cmd_uptime() {
    uint32_t ms      = timer_get_ticks();
    uint32_t seconds = ms / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours   = minutes / 60;
    seconds %= 60; minutes %= 60;
    vga_set_color(VGA_COLOR(VGA_LCYAN, VGA_BLACK));
    vga_puts(" Uptime: ");
    print_num(hours);   vga_puts("h ");
    print_num(minutes); vga_puts("m ");
    print_num(seconds); vga_puts("s (");
    print_num(ms);      vga_puts(" ms)\n");
    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
}

static void cmd_maptest() {
    vga_set_color(VGA_COLOR(VGA_LCYAN, VGA_BLACK));
    vga_puts(" Testing virtual->physical translation...\n");
    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    uint32_t *buf = (uint32_t*)kmalloc(64);
    buf[0] = 0xDEADBEEF;
    buf[1] = 0xCAFEBABE;
    uint32_t virt = (uint32_t)buf;
    uint32_t phys = paging_get_physical(virt);
    vga_puts(" Virtual  addr: 0x"); print_hex(virt); vga_putchar('\n');
    vga_puts(" Physical addr: 0x"); print_hex(phys); vga_putchar('\n');
    if (phys != 0 && (phys & ~0xFFF) == (virt & ~0xFFF)) {
        vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
        vga_puts(" Translation correct!\n");
    } else {
        vga_set_color(VGA_COLOR(VGA_LRED, VGA_BLACK));
        vga_puts(" Translation mismatch!\n");
    }
    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    vga_puts(" buf[0] = 0x"); print_hex(buf[0]); vga_putchar('\n');
    vga_puts(" buf[1] = 0x"); print_hex(buf[1]); vga_putchar('\n');
    kfree(buf);
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" maptest passed!\n");
    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
}

static void cmd_memtest() {
    vga_set_color(VGA_COLOR(VGA_LCYAN, VGA_BLACK));
    vga_puts(" Allocating 3 blocks...\n");
    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    void *a = kmalloc(128);
    void *b = kmalloc(256);
    void *c = kmalloc(512);
    if (a && b && c) {
        vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
        vga_puts(" All allocations succeeded!\n");
    } else {
        vga_set_color(VGA_COLOR(VGA_LRED, VGA_BLACK));
        vga_puts(" Allocation failed!\n");
    }
    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    mm_print_stats();
    vga_set_color(VGA_COLOR(VGA_LCYAN, VGA_BLACK));
    vga_puts(" Freeing blocks...\n");
    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    kfree(a); kfree(b); kfree(c);
    mm_print_stats();
    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
    vga_puts(" memtest passed!\n");
    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
}

static void cmd_reboot() {
    uint8_t val = 0;
    while (val & 0x02)
        __asm__ volatile ("inb $0x64, %0" : "=a"(val));
    __asm__ volatile ("outb %0, $0x64" : : "a"((uint8_t)0xFE));
    __asm__ volatile ("int $0x00");
}

static uint32_t katoi(const char *s) {
    uint32_t result = 0;
    while (*s >= '0' && *s <= '9') { result = result * 10 + (*s - '0'); s++; }
    return result;
}

static const char *parse_word(const char *buf, char *out, int max) {
    int i = 0;
    while (*buf && *buf != ' ' && i < max - 1) out[i++] = *buf++;
    out[i] = '\0';
    while (*buf == ' ') buf++;
    return buf;
}

static void handle_command() {
    cmd_buf[cmd_len] = '\0';
    vga_putchar('\n');
    if (cmd_len == 0) return;

    char verb[32];
    const char *rest = parse_word(cmd_buf, verb, 32);

    if (kstrcmp(verb, "help") == 0) {
        cmd_help();
    } else if (kstrcmp(verb, "clear") == 0) {
        vga_clear(); print_banner();
    } else if (kstrcmp(verb, "hello") == 0) {
        vga_set_color(VGA_COLOR(VGA_LCYAN, VGA_BLACK));
        vga_puts(" Hello from Mars!\n");
        vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    } else if (kstrcmp(verb, "about") == 0) {
        vga_set_color(VGA_COLOR(VGA_LCYAN, VGA_BLACK));
        vga_puts(" Mars v0.8 | 32-bit x86 | GRUB Multiboot\n");
        vga_puts(" GDT + IDT + IRQs + MM + Timer + Paging\n");
        vga_puts(" RamFS + ext2 + Scheduler + Serial + Syscalls + ELF\n");
        vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    } else if (kstrcmp(verb, "uptime") == 0) {
        cmd_uptime();
    } else if (kstrcmp(verb, "mem") == 0) {
        mm_print_stats();
    } else if (kstrcmp(verb, "memtest") == 0) {
        cmd_memtest();
    } else if (kstrcmp(verb, "paging") == 0) {
        paging_print_info();
    } else if (kstrcmp(verb, "maptest") == 0) {
        cmd_maptest();
    } else if (kstrcmp(verb, "reboot") == 0) {
        cmd_reboot();
    } else if (kstrcmp(verb, "dmesg") == 0) {
        if (klog_len == 0) {
            vga_set_color(VGA_COLOR(VGA_LGRAY, VGA_BLACK));
            vga_puts(" (no log entries)\n");
            vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
        } else {
            vga_set_color(VGA_COLOR(VGA_LGRAY, VGA_BLACK));
            for (int i = 0; i < klog_len; i++)
                vga_putchar(klog_buf[i]);
            vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
        }
    } else if (kstrcmp(verb, "ls") == 0) {
        ramfs_ls();
    } else if (kstrcmp(verb, "lsext2") == 0) {
        const char *path = (*rest == '\0') ? "/" : rest;
        ext2_list_dir(path);
    } else if (kstrcmp(verb, "create") == 0) {
        if (*rest == '\0') {
            vga_set_color(VGA_COLOR(VGA_LRED, VGA_BLACK));
            vga_puts(" Usage: create <filename>\n");
            vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
        } else {
            ramfs_create(rest);
        }
    } else if (kstrcmp(verb, "write") == 0) {
        char fname[32];
        const char *text = parse_word(rest, fname, 32);
        if (*fname == '\0' || *text == '\0') {
            vga_set_color(VGA_COLOR(VGA_LRED, VGA_BLACK));
            vga_puts(" Usage: write <filename> <text>\n");
            vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
        } else {
            ramfs_write(fname, text, (uint32_t)kstrlen(text));
        }
    } else if (kstrcmp(verb, "append") == 0) {
        char fname[32];
        const char *text = parse_word(rest, fname, 32);
        if (*fname == '\0' || *text == '\0') {
            vga_set_color(VGA_COLOR(VGA_LRED, VGA_BLACK));
            vga_puts(" Usage: append <filename> <text>\n");
            vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
        } else {
            ramfs_append(fname, text, (uint32_t)kstrlen(text));
        }
    } else if (kstrcmp(verb, "read") == 0) {
        if (*rest == '\0') {
            vga_set_color(VGA_COLOR(VGA_LRED, VGA_BLACK));
            vga_puts(" Usage: read <filename>\n");
            vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
        } else {
            ramfs_read(rest);
        }
    } else if (kstrcmp(verb, "cat") == 0) {
        if (*rest == '\0') {
            vga_set_color(VGA_COLOR(VGA_LRED, VGA_BLACK));
            vga_puts(" Usage: cat <filename>\n");
            vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
        } else {
            vfs_file_t *f = vfs_open(rest);
            if (!f) {
                vga_set_color(VGA_COLOR(VGA_LRED, VGA_BLACK));
                vga_puts(" File not found: "); vga_puts(rest); vga_putchar('\n');
                vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
            } else {
                uint8_t buf[128];
                uint32_t n;
                while ((n = vfs_read(f, buf, sizeof(buf) - 1)) > 0) {
                    for (uint32_t i = 0; i < n; i++)
                        vga_putchar((char)buf[i]);
                }
                vga_putchar('\n');
                vfs_close(f);
            }
        }
    } else if (kstrcmp(verb, "wext2") == 0) {
        char fname[64];
        const char *text = parse_word(rest, fname, 64);
        if (*fname == '\0' || *text == '\0') {
            vga_set_color(VGA_COLOR(VGA_LRED, VGA_BLACK));
            vga_puts(" Usage: wext2 <filename> <text>\n");
            vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
        } else {
            vfs_file_t *f = vfs_open_write(fname, 'w');
            if (!f) {
                vga_set_color(VGA_COLOR(VGA_LRED, VGA_BLACK));
                vga_puts(" Failed to open for write: "); vga_puts(fname); vga_putchar('\n');
                vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
            } else {
                uint32_t len = kstrlen(text);
                vfs_write(f, text, len);
                vfs_close(f);
                vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
                vga_puts(" Wrote "); print_num(len); vga_puts(" bytes to "); vga_puts(fname); vga_putchar('\n');
                vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
            }
        }
    } else if (kstrcmp(verb, "rm") == 0) {
        if (*rest == '\0') {
            vga_set_color(VGA_COLOR(VGA_LRED, VGA_BLACK));
            vga_puts(" Usage: rm <filename>\n");
            vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
        } else {
            ramfs_delete(rest);
        }
    } else if (kstrcmp(verb, "ps") == 0) {
        scheduler_list();
    } else if (kstrcmp(verb, "syscalltest") == 0) {
        vga_set_color(VGA_COLOR(VGA_LCYAN, VGA_BLACK));
        vga_puts(" Testing syscall interface...\n");
        vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
        uint32_t pid = sys_getpid();
        vga_puts(" sys_getpid()  = "); print_num(pid); vga_putchar('\n');
        uint32_t up = sys_uptime();
        vga_puts(" sys_uptime()  = "); print_num(up); vga_puts(" ms\n");
        const char *msg = " sys_write() says hello!\n";
        sys_write(msg, kstrlen(msg), 1);
        vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
        vga_puts(" All syscalls passed!\n");
        vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    } else if (kstrcmp(verb, "exec") == 0) {
        if (*rest == '\0') {
            vga_set_color(VGA_COLOR(VGA_LRED, VGA_BLACK));
            vga_puts(" Usage: exec <filename>\n");
            vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
        } else {
            vga_set_color(VGA_COLOR(VGA_LCYAN, VGA_BLACK));
            vga_puts(" Loading "); vga_puts(rest); vga_puts("...\n");
            vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
            int pid = scheduler_exec(rest);
            if (pid < 0) {
                vga_set_color(VGA_COLOR(VGA_LRED, VGA_BLACK));
                vga_puts(" Failed to exec: "); vga_puts(rest); vga_putchar('\n');
                vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
            } else {
                vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
                vga_puts(" Spawned as PID "); print_num((uint32_t)pid); vga_putchar('\n');
                vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
                scheduler_yield();
            }
        }
    } else if (kstrcmp(verb, "execd") == 0) {
        if (*rest == '\0') {
            vga_set_color(VGA_COLOR(VGA_LRED, VGA_BLACK));
            vga_puts(" Usage: execd <filename>\n");
            vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
        } else {
            char path[64];
            path[0] = '/';
            int pi = 1;
            for (int ri = 0; rest[ri] && pi < 63; ri++, pi++)
                path[pi] = rest[ri];
            path[pi] = '\0';

            uint32_t size = 0;
            uint8_t *buf = ext2_load_file(path, &size);
            if (!buf) {
                vga_set_color(VGA_COLOR(VGA_LRED, VGA_BLACK));
                vga_puts(" File not found on disk: "); vga_puts(rest); vga_putchar('\n');
                vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
            } else {
                vga_set_color(VGA_COLOR(VGA_LCYAN, VGA_BLACK));
                vga_puts(" Loading from disk: "); vga_puts(rest); vga_puts("...\n");
                vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
                int pid = scheduler_exec_buf(rest, buf, size);
                if (pid < 0) {
                    vga_set_color(VGA_COLOR(VGA_LRED, VGA_BLACK));
                    vga_puts(" Failed to exec from disk\n");
                    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
                } else {
                    vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
                    vga_puts(" Running as PID "); print_num((uint32_t)pid); vga_putchar('\n');
                    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
                    scheduler_yield();
                }
            }
        }
    } else if (kstrcmp(verb, "sleep") == 0) {
        uint32_t ms = katoi(rest);
        vga_set_color(VGA_COLOR(VGA_LCYAN, VGA_BLACK));
        vga_puts(" Sleeping for "); print_num(ms); vga_puts(" ms...\n");
        vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
        timer_sleep(ms);
        vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
        vga_puts(" Done!\n");
        vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    } else {
        vga_set_color(VGA_COLOR(VGA_LRED, VGA_BLACK));
        vga_puts(" Unknown command: "); vga_puts(cmd_buf); vga_putchar('\n');
        vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    }

    cmd_len = 0;
}

void shell_run() {
    print_banner();
    while (1) {
        vga_set_color(VGA_COLOR(VGA_LGREEN, VGA_BLACK));
        vga_puts("mars> ");
        vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
        cmd_len = 0;
        while (1) {
            char c = keyboard_read();
            if (c == '\n') {
                handle_command();
                break;
            } else if (c == '\b') {
                if (cmd_len > 0) { cmd_len--; vga_putchar('\b'); }
            } else if (cmd_len < CMD_BUF_SIZE - 1) {
                cmd_buf[cmd_len++] = c;
                vga_putchar(c);
            }
        }
    }
}
