#ifndef LINGJING_CAPABILITY_H
#define LINGJING_CAPABILITY_H

void capability_init(void);
void capability_list(void);
void capability_info(const char* name);
void capability_check(const char* name);
void capability_doctor(void);

int capability_exists(const char* name);
int capability_is_ready(const char* name);

const char* capability_get_provider(const char* name);
const char* capability_get_permission(const char* name);
const char* capability_get_status(const char* name);

#endif