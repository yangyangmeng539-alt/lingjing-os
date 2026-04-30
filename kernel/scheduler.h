#ifndef LINGJING_SCHEDULER_H
#define LINGJING_SCHEDULER_H

void scheduler_init(void);
void scheduler_tick(void);
void scheduler_yield(void);
void scheduler_reset(void);

void scheduler_list_tasks(void);
void scheduler_task_info(unsigned int id);
void scheduler_set_task_state(unsigned int id, const char* state);

void scheduler_create_task(const char* name);
void scheduler_kill_task(unsigned int id);
void scheduler_sleep_task(unsigned int id);
void scheduler_wake_task(unsigned int id);
void scheduler_set_task_priority(unsigned int id, unsigned int priority);

void scheduler_check_tasks(void);
void scheduler_doctor(void);
void scheduler_validate(void);
void scheduler_fix(void);
void scheduler_task_switch(void);
void scheduler_task_switch_check(void);
void scheduler_task_switch_doctor(void);
void scheduler_task_switch_break(void);
void scheduler_task_switch_fix(void);

int scheduler_task_switch_doctor_ok(void);

void scheduler_info(void);
void scheduler_log(void);
void scheduler_clear_log(void);
void scheduler_runqueue(void);

int scheduler_has_broken_tasks(void);
int scheduler_task_count(void);

unsigned int scheduler_get_ticks(void);
unsigned int scheduler_get_yields(void);

const char* scheduler_get_mode(void);
const char* scheduler_get_active_task(void);

#endif