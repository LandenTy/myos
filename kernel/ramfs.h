#pragma once
#include <stdint.h>
#include <stddef.h>

#define RAMFS_MAX_FILES     16
#define RAMFS_MAX_FILENAME  32
#define RAMFS_MAX_FILESIZE  65536

typedef struct {
    char     name[RAMFS_MAX_FILENAME];
    uint8_t  data[RAMFS_MAX_FILESIZE];
    uint32_t size;
    int      used;
} ramfs_file_t;

void  ramfs_init();
int   ramfs_create(const char *name);
int   ramfs_write(const char *name, const char *data, uint32_t size);
int   ramfs_append(const char *name, const char *data, uint32_t size);
int   ramfs_read(const char *name);
int   ramfs_delete(const char *name);
void  ramfs_ls();
int   ramfs_exists(const char *name);
ramfs_file_t *ramfs_get(const char *name);

