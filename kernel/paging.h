#ifndef LINGJING_PAGING_H
#define LINGJING_PAGING_H

void paging_init(void);

int paging_is_ready(void);
int paging_doctor_ok(void);

const char* paging_get_state(void);
const char* paging_get_mode(void);

unsigned int paging_get_directory_address(void);
unsigned int paging_get_table_address(void);
unsigned int paging_get_mapped_pages(void);
int paging_is_identity_mapped(unsigned int address);
void paging_map_check(unsigned int address);
void paging_enable(void);
void paging_stats(void);
void paging_flags(unsigned int address);

void paging_status(void);
void paging_check(void);
void paging_doctor(void);

void paging_break(void);
void paging_fix(void);

#endif
