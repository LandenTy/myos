#include "programs.h"
#include "ramfs.h"
#include "serial.h"
#include <stdint.h>

void programs_init() {
    // Write each program into ramfs so exec can find them
    ramfs_create("hello");
    ramfs_write("hello", (const char*)hello_data, hello_size);
    serial_puts("[programs] loaded hello into ramfs\n");

    ramfs_create("echo");
    ramfs_write("echo", (const char*)echo_data, echo_size);
    serial_puts("[programs] loaded echo into ramfs\n");

    ramfs_create("counter");
    ramfs_write("counter", (const char*)counter_data, counter_size);
    serial_puts("[programs] loaded counter into ramfs\n");
}

