#ifndef LINGJING_SYSCALL_H
#define LINGJING_SYSCALL_H

void syscall_init(void);

int syscall_is_ready(void);
int syscall_doctor_ok(void);

const char* syscall_get_state(void);
const char* syscall_get_mode(void);

unsigned int syscall_get_table_count(void);
unsigned int syscall_get_dispatch_count(void);
unsigned int syscall_get_unsupported_count(void);
unsigned int syscall_get_interrupt_count(void);

void syscall_dispatch(unsigned int id);
void syscall_interrupt_dispatch(unsigned int id);
void syscall_trigger_int80(unsigned int id);
void syscall_trigger_int80_args(unsigned int id, unsigned int arg1, unsigned int arg2, unsigned int arg3);

void syscall_table(void);
void syscall_stats(void);
void syscall_interrupt_status(void);
void syscall_frame(void);
void syscall_return_status(void);

void syscall_status(void);
void syscall_check(void);
void syscall_doctor(void);

void syscall_break(void);
void syscall_fix(void);

#endif