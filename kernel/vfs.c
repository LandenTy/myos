#include "vfs.h"
#include "ramfs.h"
#include "ext2.h"
#include "mm.h"
#include "serial.h"
#include "../include/string.h"
#include <stdint.h>

#define MAX_OPEN_FILES  64
#define WRITE_BUF_INIT  256   // initial capacity for writable files

static vfs_file_t open_files[MAX_OPEN_FILES];

static vfs_file_t *alloc_file() {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!open_files[i].in_use) {
            open_files[i].in_use   = 1;
            open_files[i].pos      = 0;
            open_files[i].size     = 0;
            open_files[i].capacity = 0;
            open_files[i].data     = 0;
            open_files[i].dirty    = 0;
            open_files[i].flags    = 0;
            return &open_files[i];
        }
    }
    return 0;
}

static void copy_path(vfs_file_t *f, const char *path) {
    int i = 0;
    while (path[i] && i < VFS_MAX_PATH - 1) { f->path[i] = path[i]; i++; }
    f->path[i] = '\0';
}

static void make_ext2_path(char *out, const char *path) {
    if (path[0] != '/') {
        out[0] = '/';
        int i = 1;
        while (path[i-1] && i < VFS_MAX_PATH - 1) { out[i] = path[i-1]; i++; }
        out[i] = '\0';
    } else {
        int i = 0;
        while (path[i] && i < VFS_MAX_PATH - 1) { out[i] = path[i]; i++; }
        out[i] = '\0';
    }
}

// ── Read-only open ────────────────────────────────────────────────────────────

vfs_file_t *vfs_open(const char *path) {
    if (!path || !path[0]) return 0;

    const char *name = path;
    if (name[0] == '/') name++;

    // Try ramfs first
    ramfs_file_t *rf = ramfs_get(name);
    if (rf && rf->used) {
        vfs_file_t *f = alloc_file();
        if (!f) return 0;
        f->data = (uint8_t*)kmalloc(rf->size + 1);
        if (!f->data) { f->in_use = 0; return 0; }
        kmemcpy(f->data, rf->data, rf->size);
        f->size     = rf->size;
        f->capacity = rf->size + 1;
        f->type     = VFS_TYPE_RAMFS;
        f->flags    = VFS_FLAG_READ;
        copy_path(f, path);
        serial_puts("[vfs] opened ramfs: "); serial_puts(name); serial_puts("\n");
        return f;
    }

    // Try ext2
    char ext2_path[VFS_MAX_PATH];
    make_ext2_path(ext2_path, path);
    uint32_t size = 0;
    uint8_t *buf = ext2_load_file(ext2_path, &size);
    if (buf && size > 0) {
        vfs_file_t *f = alloc_file();
        if (!f) { kfree(buf); return 0; }
        f->data     = buf;
        f->size     = size;
        f->capacity = size;
        f->type     = VFS_TYPE_EXT2;
        f->flags    = VFS_FLAG_READ;
        copy_path(f, path);
        serial_puts("[vfs] opened ext2: "); serial_puts(ext2_path); serial_puts("\n");
        return f;
    }

    serial_puts("[vfs] not found: "); serial_puts(path); serial_puts("\n");
    return 0;
}

// ── Write open ────────────────────────────────────────────────────────────────
// mode 'w' = truncate/create, 'a' = append

vfs_file_t *vfs_open_write(const char *path, char mode) {
    if (!path || !path[0]) return 0;

    vfs_file_t *f = alloc_file();
    if (!f) return 0;

    f->capacity = WRITE_BUF_INIT;
    f->data     = (uint8_t*)kmalloc(f->capacity);
    if (!f->data) { f->in_use = 0; return 0; }

    f->type  = VFS_TYPE_EXT2;  // will write back to ext2 on close
    f->flags = VFS_FLAG_WRITE;
    f->dirty = 1;
    copy_path(f, path);

    if (mode == 'a') {
        // Load existing content for append
        char ext2_path[VFS_MAX_PATH];
        make_ext2_path(ext2_path, path);
        uint32_t existing_size = 0;
        uint8_t *existing = ext2_load_file(ext2_path, &existing_size);
        if (existing && existing_size > 0) {
            // Grow buffer if needed
            if (existing_size >= f->capacity) {
                kfree(f->data);
                f->capacity = existing_size + WRITE_BUF_INIT;
                f->data = (uint8_t*)kmalloc(f->capacity);
                if (!f->data) { kfree(existing); f->in_use = 0; return 0; }
            }
            kmemcpy(f->data, existing, existing_size);
            f->size = existing_size;
            f->pos  = existing_size;  // append starts at end
            kfree(existing);
        }
        f->flags |= VFS_FLAG_APPEND;
        serial_puts("[vfs] opened for append: "); serial_puts(path); serial_puts("\n");
    } else {
        f->size = 0;
        f->pos  = 0;
        serial_puts("[vfs] opened for write: "); serial_puts(path); serial_puts("\n");
    }

    return f;
}

// ── Read ──────────────────────────────────────────────────────────────────────

uint32_t vfs_read(vfs_file_t *f, void *buf, uint32_t len) {
    if (!f || !f->in_use || !f->data) return 0;
    if (f->pos >= f->size) return 0;
    uint32_t avail  = f->size - f->pos;
    uint32_t to_read = (len < avail) ? len : avail;
    kmemcpy(buf, f->data + f->pos, to_read);
    f->pos += to_read;
    return to_read;
}

// ── Write ─────────────────────────────────────────────────────────────────────

uint32_t vfs_write(vfs_file_t *f, const void *buf, uint32_t len) {
    if (!f || !f->in_use || !(f->flags & VFS_FLAG_WRITE)) return 0;
    if (len == 0) return 0;

    // Grow buffer if needed
    if (f->pos + len > f->capacity) {
        uint32_t new_cap = f->pos + len + WRITE_BUF_INIT;
        uint8_t *new_buf = (uint8_t*)kmalloc(new_cap);
        if (!new_buf) return 0;
        if (f->data) {
            kmemcpy(new_buf, f->data, f->size);
            kfree(f->data);
        }
        f->data     = new_buf;
        f->capacity = new_cap;
    }

    kmemcpy(f->data + f->pos, buf, len);
    f->pos += len;
    if (f->pos > f->size) f->size = f->pos;
    f->dirty = 1;
    return len;
}

// ── Seek ──────────────────────────────────────────────────────────────────────

int vfs_seek(vfs_file_t *f, uint32_t offset) {
    if (!f || !f->in_use) return -1;
    if (offset > f->size) return -1;
    f->pos = offset;
    return 0;
}

uint32_t vfs_tell(vfs_file_t *f) {
    return (f && f->in_use) ? f->pos : 0;
}

uint32_t vfs_size(vfs_file_t *f) {
    return (f && f->in_use) ? f->size : 0;
}

// ── Flush to backing store ────────────────────────────────────────────────────

int vfs_flush(vfs_file_t *f) {
    if (!f || !f->in_use || !f->dirty) return 0;
    if (!(f->flags & VFS_FLAG_WRITE))  return 0;

    char ext2_path[VFS_MAX_PATH];
    make_ext2_path(ext2_path, f->path);

    int r = ext2_write_file(ext2_path, f->data, f->size);
    if (r == 0) {
        f->dirty = 0;
        serial_puts("[vfs] flushed: "); serial_puts(ext2_path); serial_puts("\n");
    } else {
        serial_puts("[vfs] flush failed: "); serial_puts(ext2_path); serial_puts("\n");
    }
    return r;
}

// ── Close ─────────────────────────────────────────────────────────────────────

void vfs_close(vfs_file_t *f) {
    if (!f || !f->in_use) return;
    if (f->dirty) vfs_flush(f);
    if (f->data) { kfree(f->data); f->data = 0; }
    f->in_use   = 0;
    f->size     = 0;
    f->pos      = 0;
    f->capacity = 0;
    f->dirty    = 0;
}
