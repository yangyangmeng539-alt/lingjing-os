#ifndef LINGJING_MODULE_H
#define LINGJING_MODULE_H

void module_init(void);
void module_register(const char* name, const char* status, const char* type, const char* permission, const char* depends);
void module_list(void);
void module_info(const char* name);
void module_deps(const char* name);
void module_tree(void);
void module_check(void);
int module_has_broken_dependencies(void);
void module_break_dependency(const char* name);
void module_fix_dependency(const char* name);
const char* module_get_depends(const char* name);
const char* module_get_provides(const char* name);
void module_provides(const char* name);
int module_dependency_ok(const char* name);
int module_exists(const char* name);
int module_count_loaded(void);
void module_load_mock(const char* name);
void module_unload_mock(const char* name);

#endif