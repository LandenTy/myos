global ctx_switch
global ctx_restore
global enter_userspace

ctx_switch:
    mov eax, [esp+4]    ; old_esp ptr
    mov ecx, [esp+8]    ; new_esp
    mov edx, [esp+12]   ; new_eip
    push ebp
    push ebx
    push esi
    push edi
    mov [eax], esp
    mov esp, ecx
    pop edi
    pop esi
    pop ebx
    pop ebp
    jmp edx

ctx_restore:
    mov eax, [esp+4]    ; ptr to save current esp (dead proc, don't care)
    mov ecx, [esp+8]    ; kernel esp to restore
    mov [eax], esp      ; save current esp (throwaway)
    mov esp, ecx        ; restore kernel stack
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret                 ; return to caller of enter_userspace

; enter_userspace(uint32_t *kernel_esp_save, uint32_t entry, uint32_t user_esp)
;
; Saves kernel context atomically then irets to Ring 3.
; When the process calls exit, ctx_restore reloads the saved ESP
; and ret returns to the instruction after the enter_userspace call.
enter_userspace:
    mov eax, [esp+4]    ; kernel_esp_save ptr
    mov ecx, [esp+8]    ; entry point
    mov edx, [esp+12]   ; user esp

    ; Save callee-saved registers — ctx_restore pops these on the way back
    push ebp
    push ebx
    push esi
    push edi

    ; Save ESP *after* all pushes so ctx_restore has the right value
    mov [eax], esp

    ; Switch data segments to Ring 3
    cli
    mov bx, 0x23
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx

    ; iret frame for Ring 3
    push dword 0x23     ; SS  (Ring 3 data selector)
    push edx            ; ESP (user stack top)
    push dword 0x202    ; EFLAGS (IF=1, reserved bit 1 set)
    push dword 0x1B     ; CS  (Ring 3 code selector)
    push ecx            ; EIP (entry point)
    iretd

