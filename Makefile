ASM = nasm
CC = gcc
LD = ld

CFLAGS = -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -Wall -Wextra
LDFLAGS = -m elf_i386 -T linker.ld

BUILD_DIR = build
ISO_DIR = iso

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
	$(BUILD_DIR)/timer.o \
	$(BUILD_DIR)/system.o \
	$(BUILD_DIR)/module.o \
	$(BUILD_DIR)/security.o \
	$(BUILD_DIR)/platform.o \
	$(BUILD_DIR)/lang.o \
	$(BUILD_DIR)/intent.o \
	$(BUILD_DIR)/scheduler.o

all: $(BUILD_DIR)/lingjing.iso

$(BUILD_DIR)/boot.o: boot/boot.asm
	$(ASM) -f elf32 boot/boot.asm -o $(BUILD_DIR)/boot.o

$(BUILD_DIR)/gdt_asm.o: kernel/gdt.asm
	$(ASM) -f elf32 kernel/gdt.asm -o $(BUILD_DIR)/gdt_asm.o

$(BUILD_DIR)/idt_asm.o: kernel/idt.asm
	$(ASM) -f elf32 kernel/idt.asm -o $(BUILD_DIR)/idt_asm.o

$(BUILD_DIR)/kernel.o: kernel/kernel.c
	$(CC) $(CFLAGS) -c kernel/kernel.c -o $(BUILD_DIR)/kernel.o

$(BUILD_DIR)/screen.o: kernel/screen.c
	$(CC) $(CFLAGS) -c kernel/screen.c -o $(BUILD_DIR)/screen.o

$(BUILD_DIR)/keyboard.o: kernel/keyboard.c
	$(CC) $(CFLAGS) -c kernel/keyboard.c -o $(BUILD_DIR)/keyboard.o

$(BUILD_DIR)/shell.o: kernel/shell.c
	$(CC) $(CFLAGS) -c kernel/shell.c -o $(BUILD_DIR)/shell.o

$(BUILD_DIR)/gdt.o: kernel/gdt.c
	$(CC) $(CFLAGS) -c kernel/gdt.c -o $(BUILD_DIR)/gdt.o

$(BUILD_DIR)/idt.o: kernel/idt.c
	$(CC) $(CFLAGS) -c kernel/idt.c -o $(BUILD_DIR)/idt.o

$(BUILD_DIR)/memory.o: kernel/memory.c
	$(CC) $(CFLAGS) -c kernel/memory.c -o $(BUILD_DIR)/memory.o

$(BUILD_DIR)/timer.o: kernel/timer.c
	$(CC) $(CFLAGS) -c kernel/timer.c -o $(BUILD_DIR)/timer.o

$(BUILD_DIR)/system.o: kernel/system.c
	$(CC) $(CFLAGS) -c kernel/system.c -o $(BUILD_DIR)/system.o

$(BUILD_DIR)/module.o: kernel/module.c
	$(CC) $(CFLAGS) -c kernel/module.c -o $(BUILD_DIR)/module.o

$(BUILD_DIR)/intent.o: kernel/intent.c
	$(CC) $(CFLAGS) -c kernel/intent.c -o $(BUILD_DIR)/intent.o

$(BUILD_DIR)/scheduler.o: kernel/scheduler.c
	$(CC) $(CFLAGS) -c kernel/scheduler.c -o $(BUILD_DIR)/scheduler.o

$(BUILD_DIR)/platform.o: kernel/platform.c
	$(CC) $(CFLAGS) -c kernel/platform.c -o $(BUILD_DIR)/platform.o

$(BUILD_DIR)/lang.o: kernel/lang.c
	$(CC) $(CFLAGS) -c kernel/lang.c -o $(BUILD_DIR)/lang.o

$(BUILD_DIR)/security.o: kernel/security.c
	$(CC) $(CFLAGS) -c kernel/security.c -o $(BUILD_DIR)/security.o

$(BUILD_DIR)/kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $(BUILD_DIR)/kernel.bin

$(BUILD_DIR)/lingjing.iso: $(BUILD_DIR)/kernel.bin
	cp $(BUILD_DIR)/kernel.bin $(ISO_DIR)/boot/kernel.bin
	grub-mkrescue -o $(BUILD_DIR)/lingjing.iso $(ISO_DIR)

run: all
	qemu-system-i386 -cdrom $(BUILD_DIR)/lingjing.iso -boot d -display curses

clean:
	rm -f $(BUILD_DIR)/*.o
	rm -f $(BUILD_DIR)/*.bin
	rm -f $(BUILD_DIR)/*.iso
	rm -f $(ISO_DIR)/boot/kernel.bin