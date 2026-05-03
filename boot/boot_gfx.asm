global gfx_start
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

MB2_HEADER_MAGIC equ 0xE85250D6
MB2_ARCH_I386 equ 0
MB2_HEADER_LENGTH equ mb2_header_end - mb2_header_start
MB2_CHECKSUM equ -(MB2_HEADER_MAGIC + MB2_ARCH_I386 + MB2_HEADER_LENGTH)

section .multiboot2
align 8
mb2_header_start:
    dd MB2_HEADER_MAGIC
    dd MB2_ARCH_I386
    dd MB2_HEADER_LENGTH
    dd MB2_CHECKSUM

    ; Multiboot2 framebuffer request tag
    ; type=5, flags=0, size=20, width=800, height=600, depth=32
    align 8
    dw 5
    dw 0
    dd 20
    dd 800
    dd 600
    dd 32

    ; Multiboot2 end tag
    align 8
    dw 0
    dw 0
    dd 8
mb2_header_end:

section .text
align 4
gfx_start:
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

    mov ecx, [boot_multiboot_magic]
    mov [boot_probe_magic_after_stack], ecx

    mov ecx, [boot_multiboot_info_addr]
    mov [boot_probe_info_after_stack], ecx

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