global start
global boot_multiboot_magic
global boot_multiboot_info_addr

global boot_probe_magic_initial
global boot_probe_info_initial
global boot_probe_magic_readback_before_stack
global boot_probe_info_readback_before_stack
global boot_probe_magic_after_stack
global boot_probe_info_after_stack
global boot_probe_stack_top_value

extern kernel_main

MB_MAGIC equ 0x1BADB002

; bit 0 = align modules on page boundaries
; bit 1 = provide memory information
; 不请求 video mode，避免 VGA shell 乱码
MB_FLAGS equ 0x00000003

MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

section .text
align 4
start:
    cli

    mov [boot_probe_magic_initial], eax
    mov [boot_probe_info_initial], ebx

    mov [boot_multiboot_magic], eax
    mov [boot_multiboot_info_addr], ebx

    mov ecx, [boot_multiboot_magic]
    mov [boot_probe_magic_readback_before_stack], ecx

    mov ecx, [boot_multiboot_info_addr]
    mov [boot_probe_info_readback_before_stack], ecx

    mov esp, stack_top
    mov [boot_probe_stack_top_value], esp

    mov [boot_probe_magic_after_stack], eax
    mov [boot_probe_info_after_stack], ebx

    call kernel_main

.hang:
    hlt
    jmp .hang

section .data
align 4

boot_multiboot_magic:
    dd 0

boot_multiboot_info_addr:
    dd 0

boot_probe_magic_initial:
    dd 0

boot_probe_info_initial:
    dd 0

boot_probe_magic_readback_before_stack:
    dd 0

boot_probe_info_readback_before_stack:
    dd 0

boot_probe_magic_after_stack:
    dd 0

boot_probe_info_after_stack:
    dd 0

boot_probe_stack_top_value:
    dd 0

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

section .note.GNU-stack noalloc noexec nowrite progbits