#include "ext2.h"
#include "ata.h"
#include "mm.h"
#include "serial.h"
#include "../include/vga.h"
#include "../include/string.h"
#include <stdint.h>

static ext2_superblock_t sb;
static uint32_t block_size;
static uint32_t inodes_per_group;
static uint32_t inode_size;
static int      initialized = 0;

// ── Block I/O ─────────────────────────────────────────────────────────────────

static int read_block(uint32_t block, void *buf) {
    uint32_t sectors_per_block = block_size / 512;
    uint32_t lba = block * sectors_per_block;
    return ata_read_sectors(lba, (uint8_t)sectors_per_block, buf);
}

static int write_block(uint32_t block, const void *buf) {
    uint32_t sectors_per_block = block_size / 512;
    uint32_t lba = block * sectors_per_block;
    return ata_write_sectors(lba, (uint8_t)sectors_per_block, buf);
}

// ── Init ──────────────────────────────────────────────────────────────────────

int ext2_init() {
    uint8_t buf[1024];
    if (ata_read_sectors(2, 2, buf) != 0) {
        serial_puts("[ext2] failed to read superblock\n");
        return -1;
    }

    kmemcpy(&sb, buf, sizeof(ext2_superblock_t));

    if (sb.magic != EXT2_MAGIC) {
        serial_puts("[ext2] bad magic number\n");
        return -1;
    }

    block_size       = 1024 << sb.log_block_size;
    inodes_per_group = sb.inodes_per_group;
    inode_size       = sb.rev_level >= 1 ? sb.inode_size : 128;
    initialized      = 1;

    serial_puts("[ext2] initialized, block_size=");
    serial_print_num(block_size);
    serial_puts(" inode_size=");
    serial_print_num(inode_size);
    serial_puts("\n");
    return 0;
}

// ── Group descriptor ──────────────────────────────────────────────────────────

static int read_group_desc(uint32_t group, ext2_group_desc_t *out) {
    uint8_t buf[1024];
    uint32_t gd_block = (block_size == 1024) ? 2 : 1;
    if (read_block(gd_block, buf) != 0) return -1;
    uint32_t offset = group * sizeof(ext2_group_desc_t);
    kmemcpy(out, buf + offset, sizeof(ext2_group_desc_t));
    return 0;
}

static int write_group_desc(uint32_t group, ext2_group_desc_t *gd) {
    uint8_t buf[1024];
    uint32_t gd_block = (block_size == 1024) ? 2 : 1;
    if (read_block(gd_block, buf) != 0) return -1;
    uint32_t offset = group * sizeof(ext2_group_desc_t);
    kmemcpy(buf + offset, gd, sizeof(ext2_group_desc_t));
    return write_block(gd_block, buf);
}

// ── Superblock ────────────────────────────────────────────────────────────────

static int write_superblock() {
    uint8_t buf[1024];
    kmemset(buf, 0, 1024);
    kmemcpy(buf, &sb, sizeof(ext2_superblock_t));
    return ata_write_sectors(2, 2, buf);
}

// ── Inode ─────────────────────────────────────────────────────────────────────

int ext2_read_inode(uint32_t inum, ext2_inode_t *out) {
    if (!initialized) return -1;

    uint32_t group  = (inum - 1) / inodes_per_group;
    uint32_t index  = (inum - 1) % inodes_per_group;
    uint32_t offset = index * inode_size;

    ext2_group_desc_t gd;
    if (read_group_desc(group, &gd) != 0) return -1;

    uint32_t block_offset = offset / block_size;
    uint32_t byte_offset  = offset % block_size;

    uint8_t *buf = (uint8_t*)kmalloc(block_size);
    if (!buf) return -1;

    if (read_block(gd.inode_table + block_offset, buf) != 0) {
        kfree(buf); return -1;
    }

    kmemcpy(out, buf + byte_offset, sizeof(ext2_inode_t));
    kfree(buf);
    return 0;
}

static int write_inode(uint32_t inum, ext2_inode_t *inode) {
    uint32_t group  = (inum - 1) / inodes_per_group;
    uint32_t index  = (inum - 1) % inodes_per_group;
    uint32_t offset = index * inode_size;

    ext2_group_desc_t gd;
    if (read_group_desc(group, &gd) != 0) return -1;

    uint32_t block_offset = offset / block_size;
    uint32_t byte_offset  = offset % block_size;

    uint8_t *buf = (uint8_t*)kmalloc(block_size);
    if (!buf) return -1;

    if (read_block(gd.inode_table + block_offset, buf) != 0) {
        kfree(buf); return -1;
    }
    kmemcpy(buf + byte_offset, inode, sizeof(ext2_inode_t));
    int r = write_block(gd.inode_table + block_offset, buf);
    kfree(buf);
    return r;
}

// ── Block pointer traversal ───────────────────────────────────────────────────

static int read_block_ptr(ext2_inode_t *inode, uint32_t block_idx, void *buf) {
    if (block_idx < 12) {
        return read_block(inode->block[block_idx], buf);
    }

    uint32_t ptrs_per_block = block_size / 4;

    if (block_idx < 12 + ptrs_per_block) {
        uint32_t *indirect = (uint32_t*)kmalloc(block_size);
        if (!indirect) return -1;
        if (read_block(inode->block[12], indirect) != 0) {
            kfree(indirect); return -1;
        }
        uint32_t ptr = indirect[block_idx - 12];
        kfree(indirect);
        return read_block(ptr, buf);
    }

    uint32_t di_idx = block_idx - 12 - ptrs_per_block;
    if (di_idx < ptrs_per_block * ptrs_per_block) {
        uint32_t *indirect1 = (uint32_t*)kmalloc(block_size);
        if (!indirect1) return -1;
        if (read_block(inode->block[13], indirect1) != 0) {
            kfree(indirect1); return -1;
        }
        uint32_t ptr1 = indirect1[di_idx / ptrs_per_block];
        kfree(indirect1);

        uint32_t *indirect2 = (uint32_t*)kmalloc(block_size);
        if (!indirect2) return -1;
        if (read_block(ptr1, indirect2) != 0) {
            kfree(indirect2); return -1;
        }
        uint32_t ptr2 = indirect2[di_idx % ptrs_per_block];
        kfree(indirect2);
        return read_block(ptr2, buf);
    }

    serial_puts("[ext2] triply indirect not supported\n");
    return -1;
}

// ── File read ─────────────────────────────────────────────────────────────────

int ext2_read_file(ext2_inode_t *inode, uint8_t *buf, uint32_t size) {
    if (!initialized) return -1;

    uint32_t bytes_read = 0;
    uint32_t block_idx  = 0;
    uint8_t *block_buf  = (uint8_t*)kmalloc(block_size);
    if (!block_buf) return -1;

    while (bytes_read < size) {
        if (read_block_ptr(inode, block_idx, block_buf) != 0) break;

        uint32_t to_copy = size - bytes_read;
        if (to_copy > block_size) to_copy = block_size;

        kmemcpy(buf + bytes_read, block_buf, to_copy);
        bytes_read += to_copy;
        block_idx++;
    }

    kfree(block_buf);
    return (int)bytes_read;
}

// ── Directory helpers ─────────────────────────────────────────────────────────

int ext2_find_in_dir(ext2_inode_t *dir, const char *name, uint32_t *inode_out) {
    if (!initialized) return -1;

    uint8_t *buf = (uint8_t*)kmalloc(dir->size);
    if (!buf) return -1;

    if (ext2_read_file(dir, buf, dir->size) < 0) {
        kfree(buf); return -1;
    }

    uint32_t offset = 0;
    while (offset < dir->size) {
        ext2_dirent_t *entry = (ext2_dirent_t*)(buf + offset);
        if (entry->rec_len == 0) break;
        if (entry->inode != 0) {
            int match = 1;
            for (int i = 0; i < entry->name_len; i++) {
                if (name[i] == '\0' || name[i] != entry->name[i]) {
                    match = 0; break;
                }
            }
            if (match && name[entry->name_len] == '\0') {
                *inode_out = entry->inode;
                kfree(buf);
                return 0;
            }
        }
        offset += entry->rec_len;
    }

    kfree(buf);
    return -1;
}

int ext2_open(const char *path, ext2_inode_t *out) {
    if (!initialized) return -1;
    if (path[0] != '/') return -1;

    ext2_inode_t inode;
    if (ext2_read_inode(EXT2_ROOT_INODE, &inode) != 0) return -1;

    if (path[1] == '\0') { *out = inode; return 0; }

    const char *p = path + 1;
    char component[256];

    while (*p) {
        int len = 0;
        while (p[len] && p[len] != '/') len++;
        for (int i = 0; i < len; i++) component[i] = p[i];
        component[len] = '\0';

        uint32_t next_inum;
        if (ext2_find_in_dir(&inode, component, &next_inum) != 0) return -1;
        if (ext2_read_inode(next_inum, &inode) != 0) return -1;

        p += len;
        if (*p == '/') p++;
    }

    *out = inode;
    return 0;
}

int ext2_list_dir(const char *path) {
    ext2_inode_t dir;
    if (ext2_open(path, &dir) != 0) {
        vga_puts(" ext2: path not found\n");
        return -1;
    }

    if (!(dir.mode & EXT2_S_IFDIR)) {
        vga_puts(" ext2: not a directory\n");
        return -1;
    }

    uint8_t *buf = (uint8_t*)kmalloc(dir.size);
    if (!buf) return -1;

    ext2_read_file(&dir, buf, dir.size);

    uint32_t offset = 0;
    while (offset < dir.size) {
        ext2_dirent_t *entry = (ext2_dirent_t*)(buf + offset);
        if (entry->rec_len == 0) break;
        if (entry->inode != 0) {
            for (int i = 0; i < entry->name_len; i++)
                vga_putchar(entry->name[i]);
            if (entry->file_type == EXT2_FT_DIR)
                vga_putchar('/');
            vga_putchar('\n');
        }
        offset += entry->rec_len;
    }

    kfree(buf);
    return 0;
}

uint8_t *ext2_load_file(const char *path, uint32_t *size_out) {
    ext2_inode_t inode;
    if (ext2_open(path, &inode) != 0) {
        serial_puts("[ext2] ext2_load_file: path not found: ");
        serial_puts(path); serial_puts("\n");
        return 0;
    }

    if (!(inode.mode & EXT2_S_IFREG)) {
        serial_puts("[ext2] ext2_load_file: not a regular file\n");
        return 0;
    }

    uint8_t *buf = (uint8_t*)kmalloc(inode.size);
    if (!buf) { serial_puts("[ext2] ext2_load_file: kmalloc failed\n"); return 0; }

    if (ext2_read_file(&inode, buf, inode.size) < 0) { kfree(buf); return 0; }

    if (size_out) *size_out = inode.size;

    serial_puts("[ext2] loaded: "); serial_puts(path);
    serial_puts(" size="); serial_print_num(inode.size); serial_puts("\n");
    return buf;
}

void ext2_print_info() {
    if (!initialized) { vga_puts(" ext2 not initialized\n"); return; }

    vga_set_color(VGA_COLOR(VGA_YELLOW, VGA_BLACK));
    vga_puts(" -- ext2 Info --\n");
    vga_set_color(VGA_COLOR(VGA_WHITE, VGA_BLACK));

    char buf[12]; int i; uint32_t n;

    vga_puts(" Block size  : ");
    i=11; buf[i]='\0'; n=block_size;
    while(n&&i>0){buf[--i]='0'+(n%10);n/=10;} vga_puts(&buf[i]); vga_putchar('\n');

    vga_puts(" Inode count : ");
    i=11; buf[i]='\0'; n=sb.inodes_count;
    while(n&&i>0){buf[--i]='0'+(n%10);n/=10;} vga_puts(&buf[i]); vga_putchar('\n');

    vga_puts(" Block count : ");
    i=11; buf[i]='\0'; n=sb.blocks_count;
    while(n&&i>0){buf[--i]='0'+(n%10);n/=10;} vga_puts(&buf[i]); vga_putchar('\n');

    vga_puts(" Free blocks : ");
    i=11; buf[i]='\0'; n=sb.free_blocks_count;
    while(n&&i>0){buf[--i]='0'+(n%10);n/=10;} vga_puts(&buf[i]); vga_putchar('\n');

    vga_puts(" Free inodes : ");
    i=11; buf[i]='\0'; n=sb.free_inodes_count;
    while(n&&i>0){buf[--i]='0'+(n%10);n/=10;} vga_puts(&buf[i]); vga_putchar('\n');
}

// ═════════════════════════════════════════════════════════════════════════════
// WRITE SUPPORT
// ═════════════════════════════════════════════════════════════════════════════

static uint32_t alloc_block() {
    ext2_group_desc_t gd;
    if (read_group_desc(0, &gd) != 0) return 0;
    if (gd.free_blocks_count == 0) return 0;

    uint8_t *bitmap = (uint8_t*)kmalloc(block_size);
    if (!bitmap) return 0;
    if (read_block(gd.block_bitmap, bitmap) != 0) { kfree(bitmap); return 0; }

    uint32_t total = sb.blocks_per_group;
    for (uint32_t i = 0; i < total; i++) {
        if (!(bitmap[i / 8] & (1 << (i % 8)))) {
            bitmap[i / 8] |= (1 << (i % 8));
            write_block(gd.block_bitmap, bitmap);
            kfree(bitmap);

            gd.free_blocks_count--;
            write_group_desc(0, &gd);
            sb.free_blocks_count--;
            write_superblock();

            uint32_t block_num = sb.first_data_block + i;
            uint8_t *zero = (uint8_t*)kmalloc(block_size);
            if (zero) { kmemset(zero, 0, block_size); write_block(block_num, zero); kfree(zero); }
            return block_num;
        }
    }
    kfree(bitmap);
    return 0;
}

static uint32_t alloc_inode_num() {
    ext2_group_desc_t gd;
    if (read_group_desc(0, &gd) != 0) return 0;
    if (gd.free_inodes_count == 0) return 0;

    uint8_t *bitmap = (uint8_t*)kmalloc(block_size);
    if (!bitmap) return 0;
    if (read_block(gd.inode_bitmap, bitmap) != 0) { kfree(bitmap); return 0; }

    uint32_t total = inodes_per_group;
    for (uint32_t i = 0; i < total; i++) {
        if (!(bitmap[i / 8] & (1 << (i % 8)))) {
            bitmap[i / 8] |= (1 << (i % 8));
            write_block(gd.inode_bitmap, bitmap);
            kfree(bitmap);

            gd.free_inodes_count--;
            write_group_desc(0, &gd);
            sb.free_inodes_count--;
            write_superblock();

            return i + 1;
        }
    }
    kfree(bitmap);
    return 0;
}

static void free_block(uint32_t bn) {
    if (bn == 0) return;
    ext2_group_desc_t gd;
    if (read_group_desc(0, &gd) != 0) return;

    uint8_t *bitmap = (uint8_t*)kmalloc(block_size);
    if (!bitmap) return;
    if (read_block(gd.block_bitmap, bitmap) != 0) { kfree(bitmap); return; }

    uint32_t bit = bn - sb.first_data_block;
    bitmap[bit / 8] &= ~(1 << (bit % 8));
    write_block(gd.block_bitmap, bitmap);
    kfree(bitmap);

    gd.free_blocks_count++;
    write_group_desc(0, &gd);
    sb.free_blocks_count++;
    write_superblock();
}

static int add_dirent(uint32_t dir_inum, const char *name, uint32_t inum, uint8_t file_type) {
    ext2_inode_t dir;
    if (ext2_read_inode(dir_inum, &dir) != 0) return -1;

    uint8_t *buf = (uint8_t*)kmalloc(dir.size);
    if (!buf) return -1;
    if (ext2_read_file(&dir, buf, dir.size) < 0) { kfree(buf); return -1; }

    uint8_t name_len = 0;
    while (name[name_len]) name_len++;
    uint16_t needed = (uint16_t)((8 + name_len + 3) & ~3);

    uint32_t offset = 0;
    ext2_dirent_t *last = 0;
    while (offset < dir.size) {
        ext2_dirent_t *e = (ext2_dirent_t*)(buf + offset);
        if (e->rec_len == 0) break;
        last = e;
        offset += e->rec_len;
    }

    if (!last) { kfree(buf); return -1; }

    uint16_t real_last = (uint16_t)((8 + last->name_len + 3) & ~3);
    uint16_t gap = last->rec_len - real_last;

    if (gap < needed) {
        kfree(buf);
        serial_puts("[ext2] add_dirent: directory full\n");
        return -1;
    }

    uint32_t new_offset = (uint32_t)((uint8_t*)last - buf) + real_last;
    last->rec_len = real_last;

    ext2_dirent_t *ne = (ext2_dirent_t*)(buf + new_offset);
    ne->inode     = inum;
    ne->rec_len   = gap;
    ne->name_len  = name_len;
    ne->file_type = file_type;
    for (int i = 0; i < name_len; i++) ne->name[i] = name[i];

    // Write modified directory back
    uint32_t written = 0;
    uint32_t bidx = 0;
    uint8_t *block_buf = (uint8_t*)kmalloc(block_size);
    if (!block_buf) { kfree(buf); return -1; }

    while (written < dir.size && bidx < 12) {
        uint32_t bn = dir.block[bidx];
        if (bn == 0) break;
        uint32_t to_copy = dir.size - written;
        if (to_copy > block_size) to_copy = block_size;
        kmemset(block_buf, 0, block_size);
        kmemcpy(block_buf, buf + written, to_copy);
        write_block(bn, block_buf);
        written += to_copy;
        bidx++;
    }

    kfree(block_buf);
    kfree(buf);
    return 0;
}

int ext2_write_file(const char *path, const uint8_t *data, uint32_t size) {
    if (!initialized) return -1;
    if (!path || path[0] != '/') return -1;

    const char *name = path + 1;

    ext2_inode_t inode;
    uint32_t inum = 0;
    int exists = (ext2_open(path, &inode) == 0 && (inode.mode & EXT2_S_IFREG));

    if (exists) {
        ext2_inode_t root;
        ext2_read_inode(EXT2_ROOT_INODE, &root);
        if (ext2_find_in_dir(&root, name, &inum) != 0) {
            serial_puts("[ext2] write: can't find existing inode\n");
            return -1;
        }
        for (int i = 0; i < 12; i++) {
            if (inode.block[i]) { free_block(inode.block[i]); inode.block[i] = 0; }
        }
        inode.size = 0;
    } else {
        inum = alloc_inode_num();
        if (inum == 0) { serial_puts("[ext2] write: no free inodes\n"); return -1; }
        kmemset(&inode, 0, sizeof(ext2_inode_t));
        inode.mode        = EXT2_S_IFREG | 0644;
        inode.links_count = 1;
        if (add_dirent(EXT2_ROOT_INODE, name, inum, EXT2_FT_REG_FILE) != 0) {
            serial_puts("[ext2] write: failed to add dirent\n");
            return -1;
        }
    }

    uint32_t blocks_needed = (size + block_size - 1) / block_size;
    if (blocks_needed > 12) {
        serial_puts("[ext2] write: file too large (max 12 direct blocks)\n");
        return -1;
    }

    uint8_t *block_buf = (uint8_t*)kmalloc(block_size);
    if (!block_buf) return -1;

    uint32_t written = 0;
    for (uint32_t i = 0; i < blocks_needed; i++) {
        uint32_t bn = alloc_block();
        if (bn == 0) {
            serial_puts("[ext2] write: no free blocks\n");
            kfree(block_buf); return -1;
        }
        inode.block[i] = bn;

        uint32_t to_write = size - written;
        if (to_write > block_size) to_write = block_size;
        kmemset(block_buf, 0, block_size);
        kmemcpy(block_buf, data + written, to_write);
        write_block(bn, block_buf);
        written += to_write;
    }
    kfree(block_buf);

    inode.size = size;
    if (write_inode(inum, &inode) != 0) {
        serial_puts("[ext2] write: failed to write inode\n");
        return -1;
    }

    serial_puts("[ext2] wrote: "); serial_puts(path);
    serial_puts(" size="); serial_print_num(size); serial_puts("\n");
    return 0;
}

