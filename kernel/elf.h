#pragma once
#include <stdint.h>
#include "paging.h"

#define ELF_MAGIC       0x464C457F  // "\x7FELF"
#define ET_EXEC         2
#define PT_LOAD         1
#define EM_386          3

typedef struct {
    uint32_t magic;
    uint8_t  bits;
    uint8_t  endian;
    uint8_t  version;
    uint8_t  os_abi;
    uint8_t  padding[8];
    uint16_t type;
    uint16_t machine;
    uint32_t version2;
    uint32_t entry;
    uint32_t phoff;
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed)) elf_header_t;

typedef struct {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
} __attribute__((packed)) elf_phdr_t;

// Load ELF from ramfs by name, map into current page directory
uint32_t elf_load(const char *name);

// Load ELF from a buffer, map into the provided page directory
uint32_t elf_load_buf(uint8_t *data, uint32_t size, page_directory_t *pdir);
