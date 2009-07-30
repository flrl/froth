#ifndef _VM_H
#define _VM_H

#include <stdint.h>

#include "forth.h"

extern jmp_buf abort_jmp;
extern jmp_buf quit_jmp;

static inline void execute (const pvf *xt) {
    const uint32_t *sentinel = CFA_to_SFA(xt);
    // FIXME if xt is out of our address range it can sigbus when we compare against SENTINEL
    if (xt != NULL && sentinel != NULL && *sentinel == SENTINEL) {
//        This MUST pass an argument -- here we are calling do_colon or whatever, and passing
//        in a pointer to the actual colon definition to run 
        (**xt)(CFA_to_DFA(xt));
    }
    else {
        fprintf(stderr, "Invalid execution token: %p\n", xt);
        throw(EXC_INV_ADDR);
    }
}

static inline void vm_quit() {
    fprintf(stderr, "do_quit called...\n");
    longjmp(quit_jmp, 1);
}

static inline void vm_abort() {
    fprintf(stderr, "do_abort called...\n");
    longjmp(abort_jmp, -1);
}

void catch (const pvf *);
void throw (intptr_t exception); 
void do_interpret (void *);
void do_colon (void *);
void do_constant (void *);
void do_variable (void *);
void do_value (void *);

cell getkey();
cell lastkey();
void dropline();

// Data stack macros
#define DPEEK(X)    X = stack_peek(&data_stack)
#define DPOP(X)     X = stack_pop(&data_stack)
#define DPUSH(X)    stack_push(&data_stack, (X))
#define DPICK(X)    stack_pick(&data_stack, (X))
#define DROLL(X)    stack_roll(&data_stack, (X))

// Return stack macros
#define RPEEK(X)    X = stack_peek(&return_stack)
#define RPOP(X)     X = stack_pop(&return_stack)
#define RPUSH(X)    stack_push(&return_stack, (X))
#define RPICK(X)    stack_pick(&return_stack, (X))
#define RROLL(X)    stack_roll(&return_stack, (X))

// Control stack macros
#define CPEEK(X)    X = stack_peek(&control_stack)
#define CPOP(X)     X = stack_pop(&control_stack)
#define CPUSH(X)    stack_push(&control_stack, (X))
#define CPICK(X)    stack_pick(&control_stack, (X))
#define CCOLL(X)    stack_roll(&control_stack, (X))

#define CELLALIGN(X)    (((X) + (sizeof(cell) - 1)) & ~(sizeof(cell) - 1))

#endif
