#pragma once
#include <stdint.h>
#include "paging.h"
#include "vfs.h"

#define MAX_PROCS   16
#define STACK_SIZE  8192
#define MAX_FDS     16

typedef enum {
    PROC_DEAD = 0,
    PROC_READY,
    PROC_RUNNING,
    PROC_SLEEPING,
} proc_state_t;

typedef struct {
    uint32_t          pid;
    char              name[32];
    proc_state_t      state;
    uint32_t          sleep_until;
    uint32_t          esp;
    uint32_t          eip;
    int               started;
    page_directory_t *page_dir;
    vfs_file_t       *fds[MAX_FDS];
    uint32_t          heap_end;   // current heap break (starts at 0x800000)
    uint8_t           stack[STACK_SIZE];
} process_t;

// Assembly helpers
void ctx_switch(uint32_t *old_esp, uint32_t new_esp, uint32_t eip);
void ctx_restore(uint32_t *old_esp, uint32_t new_esp);
void enter_userspace(uint32_t *kernel_esp_save, uint32_t entry, uint32_t user_esp);

void  scheduler_init();
int   scheduler_spawn(const char *name, void (*entry)());
int   scheduler_exec(const char *name);
int   scheduler_exec_buf(const char *name, uint8_t *buf, uint32_t size);
void  scheduler_exit();
void  scheduler_yield();
void  scheduler_sleep(uint32_t ms);
void  scheduler_list();
int   scheduler_get_current_pid();

// File descriptor helpers
int         fd_alloc(int pid, vfs_file_t *f);
vfs_file_t *fd_get(int pid, int fd);
void        fd_free(int pid, int fd);

// Heap helpers
uint32_t    proc_sbrk(int pid, int increment);  // returns old heap_end or -1

