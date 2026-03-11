#include "scheduler.h"
#include "elf.h"
#include "ramfs.h"
#include "paging.h"
#include "mm.h"
#include "serial.h"
#include "timer.h"
#include "gdt.h"
#include "vfs.h"
#include "../include/vga.h"
#include "../include/string.h"
#include <stdint.h>

#define USER_HEAP_START  0x00800000  // userspace heap base
#define USER_HEAP_MAX    0x01000000  // 8MB heap limit

static void copy_name(char *dst, const char *src, int max) {
    int i = 0;
    while (i < max - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static process_t procs[MAX_PROCS];
static int       current_pid = 0;
static int       proc_count  = 0;

// ── File descriptor helpers ───────────────────────────────────────────────────

int fd_alloc(int pid, vfs_file_t *f) {
    if (pid < 0 || pid >= MAX_PROCS) return -1;
    for (int i = 3; i < MAX_FDS; i++) {
        if (!procs[pid].fds[i]) {
            procs[pid].fds[i] = f;
            return i;
        }
    }
    return -1;
}

vfs_file_t *fd_get(int pid, int fd) {
    if (pid < 0 || pid >= MAX_PROCS) return 0;
    if (fd < 0   || fd >= MAX_FDS)   return 0;
    return procs[pid].fds[fd];
}

void fd_free(int pid, int fd) {
    if (pid < 0 || pid >= MAX_PROCS) return;
    if (fd < 3  || fd >= MAX_FDS)    return;
    if (procs[pid].fds[fd]) {
        vfs_close(procs[pid].fds[fd]);
        procs[pid].fds[fd] = 0;
    }
}

static void close_all_fds(int pid) {
    for (int i = 3; i < MAX_FDS; i++)
        fd_free(pid, i);
}

// ── Heap / sbrk ──────────────────────────────────────────────────────────────

uint32_t proc_sbrk(int pid, int increment) {
    if (pid < 0 || pid >= MAX_PROCS) return (uint32_t)-1;
    if (!procs[pid].page_dir)        return (uint32_t)-1;

    uint32_t old_end = procs[pid].heap_end;
    uint32_t new_end = old_end + (uint32_t)increment;

    if (new_end > USER_HEAP_MAX) {
        serial_puts("[sbrk] heap limit exceeded\n");
        return (uint32_t)-1;
    }

    // Switch to kernel directory so alloc_page_table() works safely
    paging_switch_dir(paging_get_kernel_dir());

    // Map any new pages needed into the process's page directory
    uint32_t page_start = (old_end + 0xFFF) & ~0xFFF;
    uint32_t page_end   = (new_end + 0xFFF) & ~0xFFF;

    for (uint32_t va = page_start; va < page_end; va += 0x1000) {
        paging_map_in_dir(procs[pid].page_dir, va, va,
                          PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    }

    // Switch back to the process's page directory
    paging_switch_dir(procs[pid].page_dir);

    procs[pid].heap_end = new_end;
    return old_end;
}

// ── Scheduler ────────────────────────────────────────────────────────────────

void scheduler_init() {
    for (int i = 0; i < MAX_PROCS; i++) {
        procs[i].state    = PROC_DEAD;
        procs[i].pid      = i;
        procs[i].page_dir = 0;
        procs[i].started  = 1;
        procs[i].heap_end = USER_HEAP_START;
        for (int j = 0; j < MAX_FDS; j++) procs[i].fds[j] = 0;
    }
    procs[0].state   = PROC_RUNNING;
    procs[0].started = 1;
    copy_name(procs[0].name, "kernel", 32);
    proc_count = 1;
}

int scheduler_spawn(const char *name, void (*entry)()) {
    for (int i = 1; i < MAX_PROCS; i++) {
        if (procs[i].state == PROC_DEAD) {
            procs[i].state    = PROC_READY;
            procs[i].started  = 1;
            procs[i].page_dir = 0;
            procs[i].heap_end = USER_HEAP_START;
            for (int j = 0; j < MAX_FDS; j++) procs[i].fds[j] = 0;
            copy_name(procs[i].name, name, 32);

            uint32_t *stk = (uint32_t*)(procs[i].stack + STACK_SIZE);
            *--stk = (uint32_t)entry;
            *--stk = 0; *--stk = 0; *--stk = 0; *--stk = 0;
            procs[i].esp = (uint32_t)stk;
            procs[i].eip = (uint32_t)entry;

            proc_count++;
            serial_puts("[sched] spawned: "); serial_puts(name); serial_puts("\n");
            return i;
        }
    }
    return -1;
}

static int exec_elf(const char *name, uint8_t *data, uint32_t size) {
    for (int i = 1; i < MAX_PROCS; i++) {
        if (procs[i].state == PROC_DEAD) {
            page_directory_t *pdir = paging_create_user_dir();
            if (!pdir) { serial_puts("[sched] exec: no page dir\n"); return -1; }

            paging_switch_dir(pdir);

            // Map user stack
            for (uint32_t va = 0x00F00000; va < 0x00F04000; va += 0x1000)
                paging_map_in_dir(pdir, va, va, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

            // Map initial heap page
            paging_map_in_dir(pdir, USER_HEAP_START, USER_HEAP_START,
                              PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

            uint32_t entry = elf_load_buf(data, size, pdir);
            paging_switch_dir(paging_get_kernel_dir());

            if (!entry) {
                serial_puts("[sched] exec: elf_load_buf failed\n");
                paging_destroy_user_dir(pdir);
                return -1;
            }

            procs[i].state    = PROC_READY;
            procs[i].started  = 0;
            procs[i].page_dir = pdir;
            procs[i].eip      = entry;
            procs[i].heap_end = USER_HEAP_START;
            for (int j = 0; j < MAX_FDS; j++) procs[i].fds[j] = 0;
            copy_name(procs[i].name, name, 32);

            uint32_t *stk = (uint32_t*)(procs[i].stack + STACK_SIZE);
            *--stk = 0; *--stk = 0; *--stk = 0; *--stk = 0;
            procs[i].esp = (uint32_t)stk;

            proc_count++;
            serial_puts("[sched] exec: "); serial_puts(name);
            serial_puts(" entry=0x"); serial_print_num(entry); serial_puts("\n");
            return i;
        }
    }
    serial_puts("[sched] exec: no free slots\n");
    return -1;
}

int scheduler_exec(const char *name) {
    ramfs_file_t *file = ramfs_get(name);
    if (!file) {
        serial_puts("[sched] exec: not in ramfs: "); serial_puts(name); serial_puts("\n");
        return -1;
    }
    return exec_elf(name, file->data, file->size);
}

int scheduler_exec_buf(const char *name, uint8_t *buf, uint32_t size) {
    int pid = exec_elf(name, buf, size);
    kfree(buf);
    return pid;
}

void scheduler_exit() {
    __asm__ volatile ("cli");
    procs[current_pid].state = PROC_DEAD;
    close_all_fds(current_pid);
    procs[current_pid].heap_end = USER_HEAP_START;

    if (procs[current_pid].page_dir) {
        paging_destroy_user_dir(procs[current_pid].page_dir);
        procs[current_pid].page_dir = 0;
    }

    proc_count--;
    int old = current_pid;
    current_pid = 0;
    procs[0].state = PROC_RUNNING;
    __asm__ volatile ("sti");

    paging_switch_dir(paging_get_kernel_dir());
    ctx_restore(&procs[old].esp, procs[0].esp);
}

void scheduler_yield() {
    int old  = current_pid;
    int next = -1;
    uint32_t now = timer_get_ticks();

    for (int i = 1; i < MAX_PROCS; i++) {
        if (procs[i].state == PROC_SLEEPING && now >= procs[i].sleep_until)
            procs[i].state = PROC_READY;
    }

    for (int i = 1; i < MAX_PROCS; i++) {
        int c = (old + i) % MAX_PROCS;
        if (c == 0) continue;
        if (procs[c].state == PROC_READY) { next = c; break; }
    }

    if (next == -1) {
        if (old != 0 && procs[old].state == PROC_RUNNING) return;
        if (old != 0) {
            current_pid = 0;
            procs[0].state = PROC_RUNNING;
            paging_switch_dir(paging_get_kernel_dir());
            ctx_restore(&procs[old].esp, procs[0].esp);
        }
        return;
    }

    procs[old].state  = (procs[old].state == PROC_RUNNING) ? PROC_READY : procs[old].state;
    procs[next].state = PROC_RUNNING;
    current_pid       = next;

    if (procs[next].page_dir) paging_switch_dir(procs[next].page_dir);
    else                      paging_switch_dir(paging_get_kernel_dir());

    if (!procs[next].started) {
        procs[next].started = 1;
        tss_set_kernel_stack((uint32_t)(procs[0].stack + STACK_SIZE));
        enter_userspace(&procs[0].esp, procs[next].eip, 0x00F04000);
        return;
    }

    ctx_switch(&procs[old].esp, procs[next].esp, procs[next].eip);
}

void scheduler_sleep(uint32_t ms) {
    procs[current_pid].state       = PROC_SLEEPING;
    procs[current_pid].sleep_until = timer_get_ticks() + ms;
    scheduler_yield();
}

void scheduler_list() {
    const char *states[] = { "DEAD", "READY", "RUNNING", "SLEEPING" };
    vga_set_color(VGA_COLOR(VGA_YELLOW, VGA_BLACK));
    vga_puts(" PID  STATE     NAME\n");
    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));
    for (int i = 0; i < MAX_PROCS; i++) {
        if (procs[i].state == PROC_DEAD) continue;
        vga_putchar(' '); vga_putchar('0' + i);
        vga_puts("     "); vga_puts(states[procs[i].state]);
        vga_puts("     "); vga_puts(procs[i].name); vga_putchar('\n');
    }
}

int scheduler_get_current_pid() { return current_pid; }

