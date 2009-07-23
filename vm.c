#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>

#include "exception.h"
#include "vm.h"
#include "builtin.h"

// Pre-declare a few things
extern DictEntry _dict__LIT;

/*
 *  +------+-----+------+------+-------+-------+-----+
 *  | link | len | name | code | param | param | ... |
 *  +------+-----+------+-|----+-|-----+-|-----+-----+
 *             do_colon <-/      |       \-> (code field of next word...)
 *                               V
 *           +------+-----+------+------+-------+-------+-----+
 *           | link | len | name | code | param | param | ... |
 *           +------+-----+------+-|----+-|-----+-|-----+-----+
 *                      do_colon <-/      |       \-> (code field of next word...)
 *                                        V
 *                    +------+-----+------+------+-------+-------+-----+
 *                    | link | len | name | code | param | param | ... |
 *                    +------+-----+------+-|----+-|-----+-|-----+-----+
 *                               do_colon <-/      |       \-> (code field of next word...)
 *                                                 \-> (etc)
 */

void do_catch (const pvf *xt) {
    int exception;
    ExceptionFrame *frame;

    if ((frame = exception_next_frame()) == NULL) {
        // Throw exception stack overflow exception
        fprintf(stderr, "Catch %p overflows exception stack\n", (void*) xt);
        DPUSH((cell)(intptr_t) EXC_EXOVER);
        _THROW(NULL);
    }

    frame->ds_top = data_stack.top;
    frame->rs_top = return_stack.top;
    frame->cs_top = control_stack.top;

    if ((exception = setjmp(frame->target)) == 0) {
        fprintf(stderr, "Executing xt %p with exception handler\n", (void*) xt); 

        do_execute(xt);

        // EXECUTE ran successfully, push a 0
        DPUSH((cell)(intptr_t) 0);
    }
    else {
        // Exception occurred
        fprintf (stderr, "Caught exception %i while executing %p\n", exception, xt);

        // Reset stacks
        data_stack.top = frame->ds_top;
        return_stack.top = frame->rs_top;
        control_stack.top = frame->cs_top;

        // Push the exception value
        DPUSH((cell)(intptr_t) exception);
    }

    exception_pop_frame();
}


void do_interpret (void *pfa) {
    CountedString *word;
    register cell a;

    DPUSH((cell)(intptr_t)' ');
    _WORD(NULL);

    DPEEK(a);
    word = a.as_cs;

    _FIND(NULL);
    DPOP(a); 
    if (a.as_i) {
        // Found the word in the dictionary
        DictEntry *de = a.as_de;

        if (interpreter_state == S_INTERPRET && (de->flags & F_COMPONLY)) {
            // Do nothing
            fprintf (stderr, "Useless use of \"%.*s\" in interpret mode\n", 
                (de->flags & F_LENMASK), de->name);
        }
        else if (interpreter_state == S_COMPILE && ! (de->flags & F_IMMED)) {
            // Compile it
            DPUSH(a);
            _DEtoCFA(NULL);
            _comma(NULL);
        }
        else {
            // Run it
            DPUSH(a);
            _DEtoCFA(NULL);
            DPOP(a);
            do_execute(a.as_xt); 
        }
    }
    else {
        // Word not found - try to parse a literal number out of it
        DPUSH((cell) word);
        _NUMBER(NULL);
        DPOP(a);  // Grab the return status
        if (a.as_i == 0) {
            // If there were 0 characters remaining, a number was parsed successfully
            // Value is still on the stack
            if (interpreter_state == S_COMPILE) {
                // If we're in compile mode, first compile LIT...
                DPUSH((cell) DE_to_CFA(&_dict__LIT));
                _comma(NULL);
                // ... and then compile and eat the value (which is still on the stack)
                _comma(NULL);
            }
        }
        else if (a.as_i == word->length) {
            // Couldn't parse any digits
//            error_state = E_PARSE;
//            snprintf(error_message, MAX_ERROR_LEN, 
//                "unrecognised word '%.*s'", word->length, word->value);
//            DPOP(a);  // discard junk value
//            _QUIT(NULL);
           DPUSH((cell)(intptr_t) EXC_UNDEF);
           _THROW(NULL);
        }
        else if (a.as_i > 0) {
            // Parsed some digits, but not all
            error_state = E_PARSE;
            snprintf(error_message, MAX_ERROR_LEN,
                "ignored trailing junk following number '%.*s'", word->length, word->value);
            _QUIT(NULL);  // FIXME rly?
        }
        else {
            // NUMBER failed
            error_state = E_PARSE;
            snprintf(error_message, MAX_ERROR_LEN,
                "couldn't parse number (NUMBER returned %"PRIiPTR")\n", a.as_i);
            DPOP(a);  // discard junk value
            _QUIT(NULL);
        }
    }
}


/*
    pfa     = parameter field address, contains address of parameter field
    *pfa    = parameter field, contains address of code field for next word
    **pfa   = code field, contains address of function to process the word
    ***pfa  = interpreter function for the word 
 */
void do_colon (void *pfa) {
    register cell a;
    docolon_mode = DM_NORMAL;
    cell *param = pfa;
    for (int i = 0; docolon_mode != DM_NORMAL || param[i].as_xt != 0; i++) {
        switch (docolon_mode) {
            case DM_SKIP:
                // do nothing
                docolon_mode = DM_NORMAL;
                break;
            case DM_NORMAL:
                do_execute(param[i].as_xt);
                break;
            case DM_BRANCH:
                a = param[i];       // param is an offset to branch to
                i += (a.as_i - 1);  // nb the for() increment will add the extra 1
                docolon_mode = DM_NORMAL;
                break;
            case DM_LITERAL:
                DPUSH(param[i]);
                docolon_mode = DM_NORMAL;
                break;
            case DM_SLITERAL:
                DPUSH((cell)(void*) &param[i+1]);   // start of string
                a = param[i]; DPUSH(a);             // length
                i += (CELLALIGN(a.as_i) / sizeof(cell));  
                docolon_mode = DM_NORMAL;
                break;
        }
    }
}


/*
  finds a constant in the parameter field and pushes its VALUE onto the parameter stack
*/
void do_constant (void *pfa) {
    // pfa is a pointer to the parameter field, so deref it to get the constant
    DPUSH(*((cell *) pfa));
}

/*
  finds a variable in the parameter field and pushes its ADDRESS onto the parameter stack
*/
void do_variable (void *pfa) {
    // pfa is a pointer to the parameter field, so push its value (the address) as is
    DPUSH((cell) pfa);
}

/*
  finds a value in the parameter field and pushes its VALUE onto the parameter stack
*/
void do_value (void *pfa) {
    // pfa is a pointer to the parameter field, so deref it to get the value
    DPUSH(*((cell *) pfa));
}


