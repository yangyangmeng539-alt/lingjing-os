#ifndef LINGJING_RING3_H
#define LINGJING_RING3_H

void ring3_init(void);

int ring3_is_ready(void);
int ring3_doctor_ok(void);

const char* ring3_get_state(void);
const char* ring3_get_mode(void);

void ring3_status(void);
void ring3_check(void);
void ring3_doctor(void);

void ring3_stack(void);
void ring3_frame(void);

void ring3_tss(void);
void ring3_tss_check(void);
void ring3_tss_load(void);
void ring3_tss_install(void);
void ring3_tss_clear(void);

void ring3_gdt(void);
void ring3_gdt_check(void);
void ring3_gdt_prepare(void);
void ring3_gdt_install(void);
void ring3_gdt_clear(void);

void ring3_page(void);
void ring3_page_check(void);
void ring3_page_prepare(void);
void ring3_page_clear(void);

void ring3_hw(void);
void ring3_hw_check(void);
void ring3_hw_install(void);
void ring3_hw_clear(void);

void ring3_syscall(void);
void ring3_syscall_check(void);
void ring3_syscall_prepare(void);
void ring3_syscall_clear(void);
void ring3_syscall_dryrun(void);

void ring3_syscall_stub(void);
void ring3_syscall_stub_check(void);
void ring3_syscall_stub_prepare(void);
void ring3_syscall_stub_clear(void);
void ring3_syscall_stub_select(void);
void ring3_syscall_stub_unselect(void);

void ring3_syscall_arm(void);
void ring3_syscall_disarm(void);
void ring3_syscall_exec(void);

void ring3_syscall_real(void);
void ring3_syscall_real_arm(void);
void ring3_syscall_real_disarm(void);

void ring3_syscall_gate(void);
void ring3_syscall_gate_install(void);
void ring3_syscall_gate_clear(void);
void ring3_syscall_result(void);

void ring3_guard(void);
void ring3_enter(void);
void ring3_dryrun(void);
void ring3_stub(void);
void ring3_stub_check(void);
void ring3_realenter(void);

void ring3_arm(void);
void ring3_disarm(void);

void ring3_enable_switch(void);
void ring3_disable_switch(void);

void ring3_break(void);
void ring3_fix(void);

#endif