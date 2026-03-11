global syscall_stub
extern syscall_handler

syscall_stub:
    push dword 0
    push dword 0x80

    push eax
    push ecx
    push edx
    push ebx
    push esp
    push ebp
    push esi
    push edi

    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call syscall_handler
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds

    pop edi
    pop esi
    pop ebp
    add esp, 4
    pop ebx
    pop edx
    pop ecx
    pop eax

    add esp, 8
    iret

