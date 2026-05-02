#ifndef LINGJING_INTENT_H
#define LINGJING_INTENT_H

void intent_run(const char* name);
void intent_list(void);
void intent_status(void);
void intent_context_report(void);
void intent_permissions(void);
const char* intent_get_current_name(void);
int intent_is_running(void);

#endif