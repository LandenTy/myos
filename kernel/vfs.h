#pragma once
#include <stdint.h>

#define VFS_MAX_PATH    64
#define VFS_TYPE_RAMFS  1
#define VFS_TYPE_EXT2   2

#define VFS_FLAG_READ   0x1
#define VFS_FLAG_WRITE  0x2
#define VFS_FLAG_APPEND 0x4

typedef struct {
    char     path[VFS_MAX_PATH];
    uint8_t *data;      // file contents buffer (owned by VFS)
    uint32_t size;      // current file size
    uint32_t capacity;  // allocated buffer size (for writable files)
    uint32_t pos;       // current seek position
    int      type;      // VFS_TYPE_RAMFS or VFS_TYPE_EXT2
    int      flags;     // VFS_FLAG_READ | VFS_FLAG_WRITE | VFS_FLAG_APPEND
    int      dirty;     // 1 = buffer modified, needs flush to disk on close
    int      in_use;
} vfs_file_t;

// Open existing file for reading
vfs_file_t *vfs_open(const char *path);

// Open/create file for writing ("w"=truncate, "a"=append)
vfs_file_t *vfs_open_write(const char *path, char mode);

// Read up to len bytes from current position
uint32_t vfs_read(vfs_file_t *f, void *buf, uint32_t len);

// Write len bytes at current position
uint32_t vfs_write(vfs_file_t *f, const void *buf, uint32_t len);

// Seek to absolute offset
int vfs_seek(vfs_file_t *f, uint32_t offset);

uint32_t vfs_tell(vfs_file_t *f);
uint32_t vfs_size(vfs_file_t *f);

// Flush dirty buffer to backing store (ext2/ramfs)
int vfs_flush(vfs_file_t *f);

// Close and flush
void vfs_close(vfs_file_t *f);
