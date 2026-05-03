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

const char* security_policy_mode(void);
const char* security_network_policy(void);
const char* security_external_module_policy(void);
int security_user_control_enabled(void);
int security_policy_network_blocked(void);
int security_policy_external_modules_blocked(void);
unsigned int security_policy_allowed_count(void);
unsigned int security_policy_blocked_count(void);
unsigned int security_policy_decision_count(void);
void security_policy_open(void);
void security_policy_strict(void);
void security_policy_block_network(void);
void security_policy_allow_network(void);
void security_policy_report(void);
void security_policy_check_network(void);

const char* security_file_policy(void);
const char* security_ai_policy(void);
int security_policy_file_blocked(void);
int security_policy_ai_blocked(void);
void security_policy_block_file(void);
void security_policy_allow_file(void);
void security_policy_block_ai(void);
void security_policy_allow_ai(void);
int security_check_capability(const char* cap_name, const char* permission);
void security_policy_check_capability(const char* cap_name, const char* permission);

const char* security_video_intent_policy(void);
const char* security_ai_intent_policy(void);
const char* security_network_intent_policy(void);
int security_policy_video_intent_blocked(void);
int security_policy_ai_intent_blocked(void);
void security_policy_block_video_intent(void);
void security_policy_allow_video_intent(void);
void security_policy_block_ai_intent(void);
void security_policy_allow_ai_intent(void);
void security_policy_check_intent(const char* intent_name);

void security_policy_block_external_modules(void);
void security_policy_allow_external_modules(void);
void security_policy_check_module(const char* module_name, const char* module_type);

const char* security_sandbox_profile(void);
const char* security_sandbox_state(void);
int security_sandbox_enabled(void);
unsigned int security_sandbox_switch_count(void);
void security_sandbox_profile_open(void);
void security_sandbox_profile_safe(void);
void security_sandbox_profile_locked(void);
void security_sandbox_profile_reset(void);
int security_check_sandbox(void);
void security_sandbox_report(void);

const char* security_audit_last_event(void);
const char* security_decision_summary(void);
const char* security_decision_health(void);
unsigned int security_audit_log_count(void);
void security_decision_reset_counters(void);
void security_decision_log_report(void);
void security_policy_trace_report(void);

const char* security_rule_default_policy(void);
const char* security_rule_last_target(void);
const char* security_rule_last_result(void);
unsigned int security_rule_check_count(void);
void security_rule_default_allow(void);
void security_rule_default_ask(void);
void security_rule_default_deny(void);
void security_rule_reset(void);
int security_check_rule(const char* target);
void security_rule_report(void);

#endif