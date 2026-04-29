#ifndef LINGJING_LANG_H
#define LINGJING_LANG_H

typedef enum language {
    LANG_EN = 0,
    LANG_ZH = 1
} language_t;

typedef enum message_id {
    MSG_SYSTEM_READY = 0,
    MSG_PERMISSION_DENIED,
    MSG_MODULE_NOT_FOUND,
    MSG_INTENT_RUNNING,
    MSG_DOCTOR_OK,
    MSG_LANGUAGE_CURRENT,
    MSG_LANGUAGE_SET_EN,
    MSG_LANGUAGE_SET_ZH,
    MSG_COUNT
} message_id_t;

void lang_init(void);
void lang_set(language_t lang);
language_t lang_get_current(void);
const char* lang_get(message_id_t id);
const char* lang_get_current_name(void);
int lang_doctor_ok(void);

#endif