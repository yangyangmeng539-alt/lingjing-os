#ifndef LINGJING_SECURITY_H
#define LINGJING_SECURITY_H

void security_init(void);
void security_status(void);
void security_log(void);
void security_clear_log(void);
void security_audit(const char* event);
void security_check(void);

int security_check_intent(const char* intent_name);
int security_check_module_load(const char* module_name);
int security_check_module_unload(const char* module_name, const char* module_type);
int security_doctor_ok(void);

#endif