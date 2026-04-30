#ifndef LINGJING_IDT_H
#define LINGJING_IDT_H

typedef struct registers {
    unsigned int ds;
    unsigned int edi;
    unsigned int esi;
    unsigned int ebp;
    unsigned int esp;
    unsigned int ebx;
    unsigned int edx;
    unsigned int ecx;
    unsigned int eax;
    unsigned int int_no;
    unsigned int err_code;
    unsigned int eip;
    unsigned int cs;
    unsigned int eflags;
    unsigned int useresp;
    unsigned int ss;
} registers_t;

typedef void (*interrupt_handler_t)(registers_t* regs);

void idt_init(void);
void idt_install_syscall_gate(void);
void register_interrupt_handler(unsigned char index, interrupt_handler_t handler);

#endif