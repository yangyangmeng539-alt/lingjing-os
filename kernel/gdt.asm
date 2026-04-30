global gdt_flush
global gdt_load_tss

gdt_flush:
    mov eax, [esp + 4]
    lgdt [eax]

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    jmp 0x08:.flush

.flush:
    ret

gdt_load_tss:
    mov ax, [esp + 4]
    ltr ax
    ret

section .note.GNU-stack noalloc noexec nowrite progbits