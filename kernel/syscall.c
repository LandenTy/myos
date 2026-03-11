#include "syscall.h"
#include "idt.h"
#include "irq.h"
#include "timer.h"
#include "scheduler.h"
#include "ramfs.h"
#include "vfs.h"
#include "serial.h"
#include "../include/vga.h"
#include "keyboard.h"
#include <stdint.h>

struct syscall_regs {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp_dummy;
    uint32_t ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
};

extern void syscall_stub();

void syscall_handler(struct syscall_regs *r) {
    uint32_t num  = r->eax;
    uint32_t arg0 = r->ebx;
    uint32_t arg1 = r->ecx;
    uint32_t arg2 = r->edx;
    uint32_t retval = 0;
    int pid = scheduler_get_current_pid();

    switch (num) {

        case SYSCALL_EXIT:
            serial_puts("[syscall] exit\n");
            scheduler_exit();
            break;

        case SYSCALL_GETPID:
            retval = (uint32_t)pid;
            break;

        case SYSCALL_SLEEP: {
            uint32_t until = timer_get_ticks() + arg0;
            __asm__ volatile ("sti");
            while (timer_get_ticks() < until)
                __asm__ volatile ("hlt");
            __asm__ volatile ("cli");
            retval = 0;
            break;
        }

        case SYSCALL_UPTIME:
            retval = timer_get_ticks();
            break;

        case SYSCALL_WRITE: {
            const char *str = (const char*)arg0;
            uint32_t len = arg1;
            uint8_t  fd  = (uint8_t)arg2;
            for (uint32_t i = 0; i < len; i++) {
                if (fd == 1) { vga_putchar(str[i]); serial_putchar(str[i]); }
                else if (fd == 2) { serial_putchar(str[i]); }
            }
            retval = len;
            break;
        }

        case SYSCALL_READ: {
            char    *buf = (char*)arg0;
            uint32_t len = arg1;
            for (uint32_t i = 0; i < len; i++) {
                buf[i] = keyboard_read();
                vga_putchar(buf[i]);
                if (buf[i] == '\n') { retval = i + 1; break; }
                retval = i + 1;
            }
            break;
        }

        // SYSCALL_OPEN: open file for reading
        case SYSCALL_OPEN: {
            const char *path = (const char*)arg0;
            vfs_file_t *f = vfs_open(path);
            if (!f) { retval = (uint32_t)-1; break; }
            int fd = fd_alloc(pid, f);
            if (fd < 0) { vfs_close(f); retval = (uint32_t)-1; break; }
            retval = (uint32_t)fd;
            break;
        }

        case SYSCALL_CLOSE:
            if ((int)arg0 < 3) { retval = (uint32_t)-1; break; }
            fd_free(pid, (int)arg0);
            retval = 0;
            break;

        // SYSCALL_CREATE: open file for writing; arg1 = mode char ('w' or 'a')
        case SYSCALL_CREATE: {
            const char *path = (const char*)arg0;
            char mode = (arg1 == 'a') ? 'a' : 'w';
            vfs_file_t *f = vfs_open_write(path, mode);
            if (!f) { retval = (uint32_t)-1; break; }
            int fd = fd_alloc(pid, f);
            if (fd < 0) { vfs_close(f); retval = (uint32_t)-1; break; }
            retval = (uint32_t)fd;
            break;
        }

        case SYSCALL_DELETE:
            retval = (uint32_t)ramfs_delete((const char*)arg0);
            break;

        case SYSCALL_READ_FILE: {
            vfs_file_t *f = fd_get(pid, (int)arg0);
            if (!f) { retval = (uint32_t)-1; break; }
            retval = vfs_read(f, (void*)arg1, arg2);
            break;
        }

        case SYSCALL_WRITE_FILE: {
            vfs_file_t *f = fd_get(pid, (int)arg0);
            if (!f) { retval = (uint32_t)-1; break; }
            retval = vfs_write(f, (const void*)arg1, arg2);
            break;
        }

        case SYSCALL_SEEK: {
            vfs_file_t *f = fd_get(pid, (int)arg0);
            if (!f) { retval = (uint32_t)-1; break; }
            retval = (uint32_t)vfs_seek(f, arg1);
            break;
        }

        case SYSCALL_TELL: {
            vfs_file_t *f = fd_get(pid, (int)arg0);
            if (!f) { retval = (uint32_t)-1; break; }
            retval = vfs_tell(f);
            break;
        }

        case SYSCALL_FSIZE: {
            vfs_file_t *f = fd_get(pid, (int)arg0);
            if (!f) { retval = (uint32_t)-1; break; }
            retval = vfs_size(f);
            break;
        }

        case SYSCALL_SBRK:
            retval = proc_sbrk(pid, (int)arg0);
            break;

        default:
            retval = (uint32_t)-1;
            break;
    }

    r->eax = retval;
}

void syscall_init() {
    idt_set_gate(0x80, (uint32_t)syscall_stub, 0x08, 0xEE);
    serial_puts("[Mars] Syscall interface initialized (int 0x80)\n");
}
