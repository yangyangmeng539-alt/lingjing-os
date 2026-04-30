#ifndef LINGJING_IDENTITY_H
#define LINGJING_IDENTITY_H

void identity_init(void);

int identity_is_ready(void);
int identity_doctor_ok(void);

const char* identity_get_state(void);
const char* identity_get_mode(void);
const char* identity_get_public_key(void);

void identity_status(void);
void identity_doctor(void);
void identity_check(void);

void identity_break(void);
void identity_fix(void);

#endif