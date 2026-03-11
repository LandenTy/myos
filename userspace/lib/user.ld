ENTRY(_start)
SECTIONS {
    . = 0x400000;

    .text ALIGN(4K) : {
        *(.text)
    }

    .rodata ALIGN(4K) : {
        *(.rodata)
    }

    .data ALIGN(4K) : {
        *(.data)
    }

    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss)
    }

    /* Heap starts at 0x800000 — sbrk grows it upward */
    . = 0x800000;
    _heap_start = .;
    _heap_end   = .;
}

