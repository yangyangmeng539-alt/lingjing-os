#ifndef LINGJING_HEALTH_H
#define LINGJING_HEALTH_H

void health_init(void);

int health_deps_ok(void);
int health_task_ok(void);
int health_security_ok(void);
int health_lang_ok(void);
int health_platform_ok(void);
int health_identity_ok(void);
int health_memory_ok(void);
int health_paging_ok(void);
int health_task_switch_ok(void);
int health_result_ok(void);

void health_print(void);

#endif