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

MB1_BOOTLOADER_MAGIC equ 0x2BADB002
MB1_BOOTLOADER_MAGIC_COMPAT_BAD_LOWBYTE equ 0x2BADB0FF

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

section .text
align 4
start:
    cli

    ; 先把 GRUB 交过来的原始 eax/ebx 固定住
    ; 后面不再直接依赖 eax/ebx，避免低字节异常污染 bootinfo 主诊断链
    mov esi, eax
    mov edi, ebx

    ; raw probe：保留原始现场，方便以后继续查
    mov [boot_probe_magic_initial], esi
    mov [boot_probe_info_initial], edi

    ; run-text 当前实际出现过 0x2BADB0FF
    ; 这是 Multiboot1 magic 低字节异常，不让它继续污染 bootinfo 主链
    cmp esi, MB1_BOOTLOADER_MAGIC
    je .store_clean_magic

    cmp esi, MB1_BOOTLOADER_MAGIC_COMPAT_BAD_LOWBYTE
    je .store_clean_magic

    jmp .store_raw_magic

.store_clean_magic:
    mov dword [boot_multiboot_magic], MB1_BOOTLOADER_MAGIC
    jmp .store_info

.store_raw_magic:
    mov [boot_multiboot_magic], esi

.store_info:
    mov [boot_multiboot_info_addr], edi

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