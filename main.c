#include <stdio.h>
#include <stdlib.h>

#include "forth.h"
#include "stack.h"

DictDebug junk;  // Make sure DictDebug symbol gets compiled in

Stack               data_stack;
Stack               return_stack;
Stack               control_stack;

InterpreterState    interpreter_state;
DocolonMode         docolon_mode;

Error               error_state;
char                error_message[MAX_ERROR_LEN];

jmp_buf             cold_boot;
jmp_buf             warm_boot;
ExceptionFrame      exception_frame;

extern DictEntry _dict_var_LATEST;  // This is "private" to builtin.c, but needed here to reinit

int main (int argc, char **argv) {
    int exception = 0;

    // ABORT supposedly clears the data stack and then calls QUIT
    // QUIT supposedly clears the return stack and then starts interpreting

    // ABORT jumps to here
    // FIXME how does this interact with exceptions?
    if (setjmp(cold_boot) != 0) {
        // If we get here via ABORT, discard the rest of the current input line
        register int c;
        // FIXME if the last char parsed was actually EOL this discards the NEXT line
        for (c = fgetc(stdin); c != EOF && c != '\n'; c = fgetc(stdin)) ;
        if (c == EOF)  exit(feof(stdin) ? 0 : 1);
    }

    interpreter_state = S_INTERPRET;
    docolon_mode = DM_NORMAL;
    stack_init(&data_stack);
    mem_init(0);
    var_BASE->as_i = 0;
    var_LATEST->as_de = &_dict_var_LATEST;

    // Set up a default exception handler
    exception_frame.ds_index = exception_frame.rs_index = exception_frame.cs_index = 0;
    if ((exception = setjmp(exception_frame.target)) != 0) {
        // Exception occurred
        register int c;
        fprintf(stderr, "Unhandled exception %i\n", exception);

        // Reset stacks
        data_stack.index = exception_frame.ds_index;
        return_stack.index = exception_frame.rs_index;
        control_stack.index = exception_frame.cs_index;

        // Discard rest of current input line
        // FIXME if the last char parsed was actually EOL this discards the NEXT line
        for (c = fgetc(stdin); c != EOF && c != '\n'; c = fgetc(stdin)) ;
        if (c == EOF)  exit(feof(stdin) ? 0 : 1);

        // Mark exception as having been handled
        exception = 0;
    }

    // QUIT jumps to here
    if (setjmp(warm_boot) != 0) {
        // If we get here via QUIT, discard the rest of the current input line
        register int c;
        // FIXME if the last char parsed was actually EOL this discards the NEXT line
        for (c = fgetc(stdin); c != EOF && c != '\n'; c = fgetc(stdin)) ;
        if (c == EOF)  exit(feof(stdin) ? 0 : 1);
    }
    stack_init(&return_stack);

    switch (error_state) {
        // FIXME can this stuff be done with longjmp return value stuff?
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
