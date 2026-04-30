#include "screen.h"
#include "shell.h"
#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "io.h"
#include "memory.h"
#include "timer.h"
#include "module.h"
#include "scheduler.h"
#include "security.h"
#include "lang.h"
#include "platform.h"
#include "health.h"
#include "identity.h"

unsigned int kernel_stack_marker = 0;

void kernel_main(void) {
    unsigned int local_stack_marker = 0;
    kernel_stack_marker = (unsigned int)&local_stack_marker;

    platform_clear();

    platform_print("Lingjing OS booted.\n");

    module_init();
    module_register("core", "loaded", "kernel", "system", "none");
    module_register("screen", "loaded", "driver", "display", "core");

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