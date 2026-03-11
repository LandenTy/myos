#include "elf.h"
#include "ramfs.h"
#include "paging.h"
#include "mm.h"
#include "mm.h"
#include "serial.h"
#include "../include/string.h"
#include <stdint.h>

static uint32_t do_elf_load(uint8_t *data, uint32_t size, page_directory_t *pdir) {
    if (size < sizeof(elf_header_t)) {
        serial_puts("[elf] file too small\n");
        return 0;
    }

    elf_header_t *hdr = (elf_header_t*)data;

    if (hdr->magic != ELF_MAGIC) {
        serial_puts("[elf] bad magic\n");
        return 0;
    }
    if (hdr->machine != EM_386) {
        serial_puts("[elf] not i386\n");
        return 0;
    }
    if (hdr->type != ET_EXEC) {
        serial_puts("[elf] not executable\n");
        return 0;
    }

    serial_puts("[elf] loading: entry=0x");
    serial_print_num(hdr->entry);
    serial_puts("\n");

    for (int i = 0; i < hdr->phnum; i++) {
        elf_phdr_t *ph = (elf_phdr_t*)(data + hdr->phoff + i * hdr->phentsize);
        if (ph->type != PT_LOAD) continue;

        serial_puts("[elf] segment vaddr=0x");
        serial_print_num(ph->vaddr);
        serial_puts(" filesz=");
        serial_print_num(ph->filesz);
        serial_puts(" memsz=");
        serial_print_num(ph->memsz);
        serial_puts("\n");

        uint32_t vaddr_start = ph->vaddr & ~0xFFF;
        uint32_t vaddr_end   = (ph->vaddr + ph->memsz + 0xFFF) & ~0xFFF;

        // Map pages for this segment
        for (uint32_t va = vaddr_start; va < vaddr_end; va += 0x1000) {
            if (pdir)
                paging_map_in_dir(pdir, va, va, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
            else
                paging_map(va, va, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
        }

        // Copy segment data
        kmemcpy((void*)ph->vaddr, data + ph->offset, ph->filesz);

        // Zero BSS (memsz > filesz)
        if (ph->memsz > ph->filesz) {
            kmemset((void*)(ph->vaddr + ph->filesz), 0, ph->memsz - ph->filesz);
        }
    }

    serial_puts("[elf] load complete, entry=0x");
    serial_print_num(hdr->entry);
    serial_puts("\n");

    return hdr->entry;
}

uint32_t elf_load(const char *name) {
    ramfs_file_t *file = ramfs_get(name);
    if (!file) {
        serial_puts("[elf] not found in ramfs: ");
        serial_puts(name);
        serial_puts("\n");
        return 0;
    }
    return do_elf_load(file->data, file->size, 0);
}

uint32_t elf_load_buf(uint8_t *data, uint32_t size, page_directory_t *pdir) {
    return do_elf_load(data, size, pdir);
}
