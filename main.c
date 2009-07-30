#include <stdio.h>
#include <stdlib.h>

#include "vm.h"
#include "forth.h"
#include "stack.h"

DictDebug junk;  // Make sure DictDebug symbol does not optimise out

Stack               data_stack;
Stack               return_stack;
Stack               control_stack;

jmp_buf             cold_boot;
jmp_buf             warm_boot;

int main (int argc, char **argv) {

    mem_init(0);

    // do_abort jumps to here
    if (setjmp(abort_jmp) != 0) {
        dropline();  /* discard rest of input line if we longjmp'd here */
    }

    stack_init(&data_stack, EXC_DS_UNDER, EXC_DS_OVER);

    // do_quit() jumps to here
    if (setjmp(quit_jmp) != 0) {
        dropline();  /* discard rest of input line if we longjmp'd here */
    }

    stack_init(&return_stack, EXC_RS_UNDER, EXC_RS_OVER);
    stack_init(&control_stack, EXC_CS_UNDER, EXC_CS_OVER); 
    exception_init();
    interpreter_state = S_INTERPRET;
    docolon_mode = DM_NORMAL;

    // Run the interpreter
    while (1) {
        do_interpret(NULL);
    }

    exit(0);
}
