#pragma once
#include <stdint.h>

#define SYSCALL_EXIT        0
#define SYSCALL_WRITE       1
#define SYSCALL_READ        2
#define SYSCALL_GETPID      3
#define SYSCALL_SLEEP       4
#define SYSCALL_UPTIME      5
#define SYSCALL_OPEN        6
#define SYSCALL_CLOSE       7
#define SYSCALL_CREATE      8
#define SYSCALL_DELETE      9
#define SYSCALL_READ_FILE   10
#define SYSCALL_SEEK        11
#define SYSCALL_TELL        12
#define SYSCALL_FSIZE       13
#define SYSCALL_SBRK        14
#define SYSCALL_WRITE_FILE  15

void syscall_init();
