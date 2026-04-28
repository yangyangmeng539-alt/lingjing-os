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

unsigned int kernel_stack_marker = 0;

void kernel_main(void) {
    unsigned int local_stack_marker = 0;
    kernel_stack_marker = (unsigned int)&local_stack_marker;

    screen_clear();

    screen_print("Lingjing OS booted.\n");

    module_init();
    module_register("core", "loaded", "kernel", "system", "none");
    module_register("screen", "loaded", "driver", "display", "core");

    gdt_init();
    module_register("gdt", "loaded", "arch", "system", "core");
    screen_print("GDT initialized.\n");

    idt_init();
    module_register("idt", "loaded", "arch", "system", "gdt");
    screen_print("IDT initialized.\n");

    keyboard_init();
    module_register("keyboard", "loaded", "driver", "input", "idt");
    screen_print("Keyboard IRQ initialized.\n");

    timer_init();
    module_register("timer", "loaded", "driver", "time", "idt");
    screen_print("Timer IRQ initialized.\n");

    scheduler_init();
    module_register("scheduler", "loaded", "kernel", "task", "timer");
    screen_print("Scheduler initialized.\n");

    security_init();
    module_register("security", "loaded", "kernel", "security", "core");
    screen_print("Security initialized.\n");

    memory_init();
    module_register("memory", "loaded", "kernel", "memory", "core");
    screen_print("Memory initialized.\n");

    enable_interrupts();
    screen_print("Interrupts enabled.\n");

    module_register("shell", "loaded", "userland", "command", "keyboard");
    screen_print("Shell ready.\n");

    shell_init();

    while (1) {
        shell_update();
        __asm__ volatile ("hlt");
    }
}