global _start
extern main

section .text
_start:
    and esp, 0xFFFFFFF0     ; align stack to 16 bytes
    xor ebp, ebp            ; clear base pointer
    call main
    mov ebx, eax            ; save return value
    mov eax, 0              ; SYSCALL_EXIT
    int 0x80                ; call exit
    cli
    hlt                     ; should never reach here

