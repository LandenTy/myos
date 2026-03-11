global irq0
global irq1
global irq2
global irq3
global irq4
global irq5
global irq6
global irq7
global irq8
global irq9
global irq10
global irq11
global irq12
global irq13
global irq14
global irq15

extern irq_handler

%macro IRQ 2
irq%1:
    cli
    push dword 0
    push dword %2
    jmp irq_common
%endmacro

IRQ  0,  32
IRQ  1,  33
IRQ  2,  34
IRQ  3,  35
IRQ  4,  36
IRQ  5,  37
IRQ  6,  38
IRQ  7,  39
IRQ  8,  40
IRQ  9,  41
IRQ 10,  42
IRQ 11,  43
IRQ 12,  44
IRQ 13,  45
IRQ 14,  46
IRQ 15,  47

irq_common:
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
    call irq_handler
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
    sti
    iret

