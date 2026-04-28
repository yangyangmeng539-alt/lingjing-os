#include "timer.h"
#include "idt.h"
#include "io.h"

#define PIT_FREQUENCY 1193180
#define TIMER_HZ 100

static volatile unsigned int timer_ticks = 0;

static void timer_interrupt_handler(registers_t* regs) {
    (void)regs;
    timer_ticks++;
}

void timer_init(void) {
    unsigned int divisor = PIT_FREQUENCY / TIMER_HZ;

    outb(0x43, 0x36);
    outb(0x40, (unsigned char)(divisor & 0xFF));
    outb(0x40, (unsigned char)((divisor >> 8) & 0xFF));

    register_interrupt_handler(32, timer_interrupt_handler);
}

void timer_sleep(unsigned int seconds) {
    unsigned int target_ticks = timer_ticks + seconds * TIMER_HZ;

    while (timer_ticks < target_ticks) {
        __asm__ volatile ("hlt");
    }
}

unsigned int timer_get_ticks(void) {
    return timer_ticks;
}

unsigned int timer_get_seconds(void) {
    return timer_ticks / TIMER_HZ;
}