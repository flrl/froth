#include <stdio.h>
#include <stdlib.h>

#include "vm.h"
#include "forth.h"
#include "stack.h"

DictDebug junk;  // Make sure DictDebug symbol does not optimise out

Stack               data_stack;
Stack               return_stack;
Stack               control_stack;

InterpreterState    interpreter_state;
DocolonMode         docolon_mode;

Error               error_state;
char                error_message[MAX_ERROR_LEN];

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

    switch (error_state) {
        // FIXME ditch this and use the exception stuff
        case E_OK: break;
        case E_PARSE:
            fprintf(stderr, "Parse error: %s\n", error_message);
            break;
    }

    error_state = E_OK;
    memset(error_message, 0, sizeof(error_message));

    // Run the interpreter
    while (1) {
        do_interpret(NULL);
    }

    exit(0);
}
