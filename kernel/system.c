#include "system.h"
#include "io.h"

void system_reboot(void) {
    unsigned char good = 0x02;

    while ((good & 0x02) != 0) {
        good = inb(0x64);
    }

    outb(0x64, 0xFE);

    while (1) {
        __asm__ volatile ("hlt");
    }
}

void system_halt(void) {
    disable_interrupts();

    while (1) {
        __asm__ volatile ("hlt");
    }
}