ASM = nasm
CC = gcc
LD = ld

CFLAGS = -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -Wall -Wextra
LDFLAGS = -m elf_i386 -T linker.ld

BUILD_DIR = build
ISO_DIR = iso
TEXT_ISO_DIR = $(BUILD_DIR)/iso-text
GFX_ISO_DIR = $(BUILD_DIR)/iso-gfx

OBJS = \
	$(BUILD_DIR)/boot.o \
	$(BUILD_DIR)/kernel.o \
	$(BUILD_DIR)/screen.o \
	$(BUILD_DIR)/keyboard.o \
	$(BUILD_DIR)/shell.o \
	$(BUILD_DIR)/gdt.o \
	$(BUILD_DIR)/gdt_asm.o \
	$(BUILD_DIR)/idt.o \
	$(BUILD_DIR)/idt_asm.o \
	$(BUILD_DIR)/memory.o \
	$(BUILD_DIR)/paging.o \
	$(BUILD_DIR)/timer.o \
	$(BUILD_DIR)/system.o \
	$(BUILD_DIR)/module.o \
	$(BUILD_DIR)/capability.o \
	$(BUILD_DIR)/bootinfo.o \
	$(BUILD_DIR)/framebuffer.o \
	$(BUILD_DIR)/graphics.o \
	$(BUILD_DIR)/gshell.o \
	$(BUILD_DIR)/security.o \
	$(BUILD_DIR)/syscall.o \
	$(BUILD_DIR)/user.o \
	$(BUILD_DIR)/ring3.o \
	$(BUILD_DIR)/ring3_asm.o \
	$(BUILD_DIR)/platform.o \
	$(BUILD_DIR)/lang.o \
	$(BUILD_DIR)/identity.o \
	$(BUILD_DIR)/health.o \
	$(BUILD_DIR)/intent.o \
	$(BUILD_DIR)/scheduler.o

KERNEL_OBJS = $(filter-out $(BUILD_DIR)/boot.o,$(OBJS))

all: $(BUILD_DIR)/lingjing.iso

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/boot.o: boot/boot.asm | $(BUILD_DIR)
	$(ASM) -f elf32 boot/boot.asm -o $(BUILD_DIR)/boot.o

$(BUILD_DIR)/boot_gfx.o: boot/boot_gfx.asm | $(BUILD_DIR)
	$(ASM) -f elf32 boot/boot_gfx.asm -o $(BUILD_DIR)/boot_gfx.o

$(BUILD_DIR)/gdt_asm.o: kernel/gdt.asm | $(BUILD_DIR)
	$(ASM) -f elf32 kernel/gdt.asm -o $(BUILD_DIR)/gdt_asm.o

$(BUILD_DIR)/idt_asm.o: kernel/idt.asm | $(BUILD_DIR)
	$(ASM) -f elf32 kernel/idt.asm -o $(BUILD_DIR)/idt_asm.o

$(BUILD_DIR)/kernel.o: kernel/kernel.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/kernel.c -o $(BUILD_DIR)/kernel.o

$(BUILD_DIR)/screen.o: kernel/screen.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/screen.c -o $(BUILD_DIR)/screen.o

$(BUILD_DIR)/keyboard.o: kernel/keyboard.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/keyboard.c -o $(BUILD_DIR)/keyboard.o

$(BUILD_DIR)/shell.o: kernel/shell.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/shell.c -o $(BUILD_DIR)/shell.o

$(BUILD_DIR)/gdt.o: kernel/gdt.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/gdt.c -o $(BUILD_DIR)/gdt.o

$(BUILD_DIR)/idt.o: kernel/idt.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/idt.c -o $(BUILD_DIR)/idt.o

$(BUILD_DIR)/memory.o: kernel/memory.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/memory.c -o $(BUILD_DIR)/memory.o

$(BUILD_DIR)/paging.o: kernel/paging.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/paging.c -o $(BUILD_DIR)/paging.o

$(BUILD_DIR)/timer.o: kernel/timer.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/timer.c -o $(BUILD_DIR)/timer.o

$(BUILD_DIR)/system.o: kernel/system.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/system.c -o $(BUILD_DIR)/system.o

$(BUILD_DIR)/module.o: kernel/module.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/module.c -o $(BUILD_DIR)/module.o

$(BUILD_DIR)/capability.o: kernel/capability.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/capability.c -o $(BUILD_DIR)/capability.o

$(BUILD_DIR)/bootinfo.o: kernel/bootinfo.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/bootinfo.c -o $(BUILD_DIR)/bootinfo.o

$(BUILD_DIR)/framebuffer.o: kernel/framebuffer.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/framebuffer.c -o $(BUILD_DIR)/framebuffer.o

$(BUILD_DIR)/graphics.o: kernel/graphics.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/graphics.c -o $(BUILD_DIR)/graphics.o

$(BUILD_DIR)/gshell.o: kernel/gshell.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/gshell.c -o $(BUILD_DIR)/gshell.o

$(BUILD_DIR)/health.o: kernel/health.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/health.c -o $(BUILD_DIR)/health.o

$(BUILD_DIR)/intent.o: kernel/intent.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/intent.c -o $(BUILD_DIR)/intent.o

$(BUILD_DIR)/scheduler.o: kernel/scheduler.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/scheduler.c -o $(BUILD_DIR)/scheduler.o

$(BUILD_DIR)/platform.o: kernel/platform.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/platform.c -o $(BUILD_DIR)/platform.o

$(BUILD_DIR)/lang.o: kernel/lang.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/lang.c -o $(BUILD_DIR)/lang.o

$(BUILD_DIR)/identity.o: kernel/identity.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/identity.c -o $(BUILD_DIR)/identity.o

$(BUILD_DIR)/security.o: kernel/security.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/security.c -o $(BUILD_DIR)/security.o

$(BUILD_DIR)/syscall.o: kernel/syscall.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/syscall.c -o $(BUILD_DIR)/syscall.o

$(BUILD_DIR)/user.o: kernel/user.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/user.c -o $(BUILD_DIR)/user.o

$(BUILD_DIR)/ring3.o: kernel/ring3.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c kernel/ring3.c -o $(BUILD_DIR)/ring3.o

$(BUILD_DIR)/ring3_asm.o: kernel/ring3.asm | $(BUILD_DIR)
	$(ASM) -f elf32 kernel/ring3.asm -o $(BUILD_DIR)/ring3_asm.o

$(BUILD_DIR)/kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $(BUILD_DIR)/kernel.bin

$(BUILD_DIR)/kernel_gfx.bin: $(BUILD_DIR)/boot_gfx.o $(KERNEL_OBJS)
	$(LD) $(LDFLAGS) -e gfx_start $(BUILD_DIR)/boot_gfx.o $(KERNEL_OBJS) -o $(BUILD_DIR)/kernel_gfx.bin

$(BUILD_DIR)/lingjing.iso: $(BUILD_DIR)/kernel.bin
	rm -rf $(TEXT_ISO_DIR)
	mkdir -p $(TEXT_ISO_DIR)
	cp -r $(ISO_DIR)/* $(TEXT_ISO_DIR)/
	cp $(BUILD_DIR)/kernel.bin $(TEXT_ISO_DIR)/boot/kernel.bin
	cp $(ISO_DIR)/boot/grub/grub.cfg $(TEXT_ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(BUILD_DIR)/lingjing.iso $(TEXT_ISO_DIR)

run: run-gfx

run-text: all
	qemu-system-i386 -cdrom $(BUILD_DIR)/lingjing.iso -boot d -display curses

run-gfx: all
	qemu-system-i386 -cdrom $(BUILD_DIR)/lingjing.iso -boot d -vga std

gfxboot-iso: $(BUILD_DIR)/kernel_gfx.bin
	rm -rf $(GFX_ISO_DIR)
	mkdir -p $(GFX_ISO_DIR)
	cp -r $(ISO_DIR)/* $(GFX_ISO_DIR)/
	cp $(BUILD_DIR)/kernel_gfx.bin $(GFX_ISO_DIR)/boot/kernel.bin
	cp $(ISO_DIR)/boot/grub/grub_gfxboot.cfg $(GFX_ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(BUILD_DIR)/lingjing-gfxboot.iso $(GFX_ISO_DIR)

run-gfxboot: gfxboot-iso
	qemu-system-i386 -cdrom $(BUILD_DIR)/lingjing-gfxboot.iso -boot d -display gtk -device VGA,vgamem_mb=16 -no-reboot -no-shutdown

clean:
	rm -f $(BUILD_DIR)/*.o
	rm -f $(BUILD_DIR)/*.bin
	rm -f $(BUILD_DIR)/*.iso
	rm -rf $(TEXT_ISO_DIR)
	rm -rf $(GFX_ISO_DIR)
	rm -f $(ISO_DIR)/boot/kernel.bin