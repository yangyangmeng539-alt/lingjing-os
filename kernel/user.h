#ifndef LINGJING_USER_H
#define LINGJING_USER_H

void user_init(void);

int user_is_ready(void);
int user_doctor_ok(void);

const char* user_get_state(void);
const char* user_get_mode(void);

unsigned int user_get_program_count(void);
unsigned int user_get_entry_count(void);

void user_status(void);
void user_check(void);
void user_doctor(void);
void user_programs(void);
void user_entries(void);
void user_stats(void);

void user_break(void);
void user_fix(void);

#endif