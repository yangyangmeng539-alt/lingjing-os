global ring3_enter_user_mode
global ring3_user_stub_asm
global ring3_user_syscall_stub_asm

; void ring3_enter_user_mode(unsigned int eip,
;                            unsigned int user_cs,
;                            unsigned int eflags,
;                            unsigned int user_esp,
;                            unsigned int user_ds);
;
; cdecl stack:
; [esp + 4]  = eip
; [esp + 8]  = user_cs
; [esp + 12] = eflags
; [esp + 16] = user_esp
; [esp + 20] = user_ds

ring3_enter_user_mode:
    cli

    mov eax, [esp + 4]      ; eip
    mov ebx, [esp + 8]      ; user_cs
    mov ecx, [esp + 12]     ; eflags
    mov edx, [esp + 16]     ; user_esp
    mov esi, [esp + 20]     ; user_ds / user_ss

    push esi                ; ss
    push edx                ; esp
    push ecx                ; eflags
    push ebx                ; cs
    push eax                ; eip

    iretd

.hang:
    jmp .hang


; 安全 ring3 死循环 stub
ring3_user_stub_asm:
.user_loop:
    jmp .user_loop


; ring3 syscall stub
; 先不要默认跳这里，必须由 ring3 syscallstubselect 切换 entry
ring3_user_syscall_stub_asm:
    mov eax, 0
    mov ebx, 11
    mov ecx, 22
    mov edx, 33

    int 0x80

.syscall_loop:
    jmp .syscall_loop

section .note.GNU-stack noalloc noexec nowrite progbits