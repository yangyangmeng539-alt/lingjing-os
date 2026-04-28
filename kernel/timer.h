#ifndef LINGJING_TIMER_H
#define LINGJING_TIMER_H

void timer_init(void);
void timer_sleep(unsigned int seconds);
unsigned int timer_get_ticks(void);
unsigned int timer_get_seconds(void);

#endif