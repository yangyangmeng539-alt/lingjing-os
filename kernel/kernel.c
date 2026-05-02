#include "screen.h"
#include "shell.h"
#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "io.h"
#include "memory.h"
#include "paging.h"
#include "timer.h"
#include "module.h"
#include "capability.h"
#include "bootinfo.h"
#include "framebuffer.h"
#include "graphics.h"
#include "gshell.h"
#include "scheduler.h"
#include "security.h"
#include "syscall.h"
#include "user.h"
#include "ring3.h"
#include "lang.h"
#include "platform.h"
#include "health.h"
#include "identity.h"

extern unsigned int boot_multiboot_magic;
extern unsigned int boot_multiboot_info_addr;

unsigned int kernel_stack_marker = 0;

void kernel_main(void) {
    unsigned int local_stack_marker = 0;
    kernel_stack_marker = (unsigned int)&local_stack_marker;

    platform_clear();

    platform_print("Lingjing OS booted.\n");

    bootinfo_init(boot_multiboot_magic, boot_multiboot_info_addr);

    module_init();
    module_register("core", "loaded", "kernel", "system", "none");
    module_register("screen", "loaded", "driver", "display", "core");
    module_register("bootinfo", "loaded", "kernel", "boot", "core");
    platform_print("Bootinfo initialized.\n");

    gdt_init();
    module_register("gdt", "loaded", "arch", "system", "core");
    platform_print("GDT initialized.\n");

    idt_init();
    module_register("idt", "loaded", "arch", "system", "gdt");
    platform_print("IDT initialized.\n");

    keyboard_init();
    module_register("keyboard", "loaded", "driver", "input", "idt");
    platform_print("Keyboard IRQ initialized.\n");

    timer_init();
    module_register("timer", "loaded", "driver", "time", "idt");
    platform_print("Timer IRQ initialized.\n");

    scheduler_init();
    module_register("scheduler", "loaded", "kernel", "task", "timer");
    platform_print("Scheduler initialized.\n");

    security_init();
    module_register("security", "loaded", "kernel", "security", "core");
    platform_print("Security initialized.\n");

    syscall_init();
    module_register("syscall", "loaded", "kernel", "syscall", "security");
    platform_print("Syscall initialized.\n");

    user_init();
    module_register("user", "loaded", "kernel", "user", "syscall");
    platform_print("User mode initialized.\n");

    ring3_init();
    module_register("ring3", "loaded", "kernel", "ring3", "user");
    platform_print("Ring3 metadata initialized.\n");

    capability_init();
    module_register("capability", "loaded", "kernel", "capability", "core");
    platform_print("Capability registry initialized.\n");

    framebuffer_init();
    module_register("framebuffer", "loaded", "kernel", "display", "screen");
    platform_print("Framebuffer metadata initialized.\n");

    graphics_init();
    module_register("graphics", "loaded", "kernel", "display", "framebuffer");
    platform_print("Graphics metadata initialized.\n");

    gshell_init();
    module_register("gshell", "loaded", "kernel", "display", "graphics");
    platform_print("Graphical shell metadata initialized.\n");

    platform_init();
    module_register("platform", "loaded", "kernel", "platform", "core");
    platform_print("Platform initialized.\n");

    lang_init();
    module_register("lang", "loaded", "kernel", "language", "core");
    platform_print("Language initialized.\n");

    identity_init();
    module_register("identity", "loaded", "kernel", "identity", "security");
    platform_print("Identity initialized.\n");

    health_init();
    module_register("health", "loaded", "kernel", "diagnostic", "core");
    platform_print("Health initialized.\n");

    memory_init();
    module_register("memory", "loaded", "kernel", "memory", "core");
    platform_print("Memory initialized.\n");

    paging_init();
    module_register("paging", "loaded", "kernel", "paging", "memory");
    platform_print("Paging initialized.\n");

    enable_interrupts();
    platform_print("Interrupts enabled.\n");

    module_register("shell", "loaded", "userland", "command", "keyboard");
    platform_print("Shell ready.\n");

    shell_init();

    while (1) {
        shell_update();
        __asm__ volatile ("hlt");
    }
}