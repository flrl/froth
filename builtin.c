#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "forth.h"
#include "stack.h"

// Function signature for a primitive
#define DECLARE_PRIMITIVE(P)    void P (void *pfa)

// Define a primitive and add it to the dictionary
#define PRIMITIVE(NAME, FLAGS, CNAME, LINK)                                         \
    DECLARE_PRIMITIVE(CNAME);                                                       \
    DictEntry _dict_##CNAME =                                                       \
        { &_dict_##LINK, ((FLAGS) | (sizeof(NAME) - 1)), NAME, SENTINEL, CNAME, };  \
    DECLARE_PRIMITIVE(CNAME)

// Define a variable and add it to the dictionary; also create a pointer for direct access
#define VARIABLE(NAME, INITIAL, FLAGS, LINK)                        \
    DictEntry _dict_var_##NAME =                                    \
        { &_dict_##LINK, ((FLAGS) | (sizeof(#NAME) - 1)), #NAME,    \
            SENTINEL, do_variable, {INITIAL} };                     \
    cell * const var_##NAME = &_dict_var_##NAME.param[0]

// Define a constant and add it to the dictionary; also create a pointer for direct access
#define CONSTANT(NAME, VALUE, FLAGS, LINK)                          \
    DictEntry _dict_const_##NAME =                                  \
        { &_dict_##LINK, ((FLAGS) | (sizeof(#NAME) - 1)), #NAME,    \
            SENTINEL, do_constant, { VALUE } };                     \
    const cell * const const_##NAME = &_dict_const_##NAME.param[0]

// Define a "read only" variable and add it to the dictionary (special case of PRIMITIVE)
#define READONLY(NAME, CELLFUNC, FLAGS, LINK)                       \
    DECLARE_PRIMITIVE(readonly_##NAME);                             \
    DictEntry _dict_readonly_##NAME =                               \
        { &_dict_##LINK, ((FLAGS) | (sizeof(#NAME) - 1)), #NAME,    \
            SENTINEL, readonly_##NAME, };                           \
    DECLARE_PRIMITIVE(readonly_##NAME) { REG(a); a = (CELLFUNC); PPUSH(a); }

// Shorthand macros to make repetitive code more writeable, but possibly less readable
#define REG(X)          register cell X
#define PTOP(X)         X = stack_top(&parameter_stack)
#define PPOP(X)         X = stack_pop(&parameter_stack)
#define PPUSH(X)        stack_push(&parameter_stack, (X))
#define CELLALIGN(X)    (((X) + (sizeof(cell) - 1)) & ~(sizeof(cell) - 1))
#define RTOP(X)         X = stack_top(&return_stack)
#define RPOP(X)         X = stack_pop(&return_stack)
#define RPUSH(X)        stack_push(&return_stack, (X))


// pre-declare a few things
DictEntry _dict__LIT;


// FIXME do i want these all to take a void* argument, or should they just use a global register
// W to read the pfa from?

// all primitives are called with a parameter, which is a pointer to the parameter field
// of the word definition they're called from.
// for most primitives, this is simply ignored, as all their data comes from the stack
// for special primitives eg do_colon, do_constant etc, this is needed to get the inner contents
// of the word definition.


                      
/*
+------+-----+------+------+-------+-------+-----+
| link | len | name | code | param | param | ... |
+------+-----+------+-|----+-|-----+-|-----+-----+
           do_colon <-/      |       \-> (code field of next word...)
                             V
        +------+-----+------+------+-------+-------+-----+
        | link | len | name | code | param | param | ... |
        +------+-----+------+-|----+-|-----+-|-----+-----+
                   do_colon <-/      |       \-> (code field of next word...)
                                     V
                +------+-----+------+------+-------+-------+-----+
                | link | len | name | code | param | param | ... |
                +------+-----+------+-|----+-|-----+-|-----+-----+
                           do_colon <-/      |       \-> (code field of next word...)
                                             \-> (etc)

*/
static inline void do_execute (const pvf *xt, void *arg) {
    uint32_t *sentinel = (uint32_t*) (xt) - 1;
    if (*sentinel == SENTINEL) {
        (**xt)(arg);
    }
    else {
        fprintf(stderr, "Invalid execution token: %p\n", xt);
    }
}


void do_interpret (void *pfa) {
    REG(addr);
    REG(len);
    REG(a);

    PPUSH(' ');
    _WORD(NULL);

    _2DUP(NULL);
    PPOP(len);
    PPOP(addr);

    _FIND(NULL);
    PPOP(a); 
    if (a) {
        // Found the word in the dictionary
        DictEntry *de = (DictEntry *) a;
        uint8_t flags = de->name_length & ~F_LENMASK;

        if (interpreter_state == S_INTERPRET && (flags & F_NOINTERP)) {
            // Do nothing
            fprintf (stderr, "Useless use of \"%.*s\" in interpret mode\n", 
                (de->name_length & F_LENMASK), de->name);
        }
        else if (interpreter_state == S_COMPILE && ! (flags & F_IMMED)) {
            // Compile it
            PPUSH(a);
            _toCFA(NULL);
            _comma(NULL);
        }
        else {
            // Run it
            PPUSH(a);
            _toCFA(NULL);
            PPOP(a);
            do_execute ((pvf *) a, (void*) (a + sizeof(cell)));
        }
    }
    else {
        // Word not found - try to parse a literal number out of it
        PPUSH(addr);
        PPUSH(len);
        _NUMBER(NULL);
        PPOP(a);  // Grab the return status
        if (a == 0) {
            // If there were 0 characters remaining, a number was parsed successfully
            // Value is still on the stack
            if (interpreter_state == S_COMPILE) {
                // If we're in compile mode, first compile LIT...
                PPUSH((cell) DE_to_CFA(&_dict__LIT));
                _comma(NULL);
                // ... and then the value (which is still on the stack)
                _comma(NULL);
            }
        }
        else if (a == len) {
            // Couldn't parse any digits
            error_state = E_PARSE;
            snprintf(error_message, MAX_ERROR_LEN, 
                "unrecognised word '%.*s'", (int) len, (const char *) addr);
            PPOP(a);  // discard junk value
            _QUIT(NULL);
        }
        else {
            // Parsed some digits, but not all
            error_state = E_PARSE;
            snprintf(error_message, MAX_ERROR_LEN,
                "ignored trailing junk following number '%.*s'", (int) len, (const char *) addr);
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
void do_colon(void *pfa) {
    REG(a);
    REG(b);
    docolon_mode = DM_NORMAL;
//    cell *param = pfa;
    new_cell *param = pfa;
    for (int i = 0; docolon_mode != DM_NORMAL || param[i].cfa != 0; i++) {
        switch (docolon_mode) {
            case DM_SKIP:
                // do nothing
                docolon_mode = DM_NORMAL;
                break;
            case DM_NORMAL:
                do_execute(param[i].cfa, param[i].pfa + 1);
//                (**param[i].cfa) (param[i].pfa + 1);
                break;
            case DM_BRANCH:
                a = param[i].i; // param is an offset to branch to
                i += (a - 1);   // nb the for() increment will add the extra 1
                docolon_mode = DM_NORMAL;
                break;
            case DM_LITERAL:
                PPUSH((cell)param[i].i);
                docolon_mode = DM_NORMAL;
                break;
            case DM_SLITERAL:
                a = param[i].u;         // length
                b = (cell) &param[i+1]; // start of string
                PPUSH(b);
                PPUSH(a);
                i += (CELLALIGN(a) / sizeof(cell));  
                docolon_mode = DM_NORMAL;
                break;
        }
    }
}


/*
  finds a constant in the parameter field and pushes its VALUE onto the parameter stack
 */
void do_constant(void *pfa) {
    // pfa is a pointer to the parameter field, so deref it to get the constant
    PPUSH(*((cell *) pfa));
}

/*
  finds a variable in the parameter field and pushes its ADDRESS onto the parameter stack
*/
void do_variable(void *pfa) {
    // pfa is a pointer to the parameter field, so push its value (the address) as is
    PPUSH((cell) pfa);
}



/***************************************************************************
    The __ROOT DictEntry delimits the root of the dictionary
 ***************************************************************************/
DictEntry _dict___ROOT = {
    NULL,   // link
    0,      // name length + flags
    "",     // name
    0,      // sentinel
    NULL,   // code
};


/***************************************************************************
  Builtin variables -- keep these together
  Exception: LATEST should be declared very last (see end of file)
    VARIABLE(NAME, INITIAL, FLAGS, LINK)
***************************************************************************/
VARIABLE (BASE,     0,              0,          __ROOT);        // default to smart base
VARIABLE (UINCR,    INIT_UINCR,     0,          var_BASE);      //
VARIABLE (UTHRES,   INIT_UTHRES,    0,          var_UINCR);     //
VARIABLE (HERE,     0,              0,          var_UTHRES);    // default to NULL


/***************************************************************************
  Builtin constants -- keep these together
    CONSTANT(NAME, VALUE, FLAGS, LINK)
 ***************************************************************************/
CONSTANT (VERSION,      0,                      0,  var_HERE);
CONSTANT (DOCOL,        (cell) &do_colon,       0,  const_VERSION);
CONSTANT (DOVAR,        (cell) &do_variable,    0,  const_DOCOL);
CONSTANT (DOCON,        (cell) &do_constant,    0,  const_DOVAR);
CONSTANT (EXIT,         0,                      0,  const_DOCON);
CONSTANT (F_IMMED,      F_IMMED,                0,  const_EXIT);
CONSTANT (F_NOINTERP,   F_NOINTERP,             0,  const_F_IMMED);
CONSTANT (F_HIDDEN,     F_HIDDEN,               0,  const_F_NOINTERP);
CONSTANT (F_LENMASK,    F_LENMASK,              0,  const_F_HIDDEN);
CONSTANT (S_INTERPRET,  S_INTERPRET,            0,  const_F_LENMASK);
CONSTANT (S_COMPILE,    S_COMPILE,              0,  const_S_INTERPRET);
CONSTANT (CELL_MIN,     INTPTR_MIN,             0,  const_S_COMPILE);
CONSTANT (CELL_MAX,     INTPTR_MAX,             0,  const_CELL_MIN);
CONSTANT (UCELL_MIN,    0,                      0,  const_CELL_MAX);
CONSTANT (UCELL_MAX,    UINTPTR_MAX,            0,  const_UCELL_MIN);
//CONSTANT (SHEEP,        0xDEADBEEF,             0,  const_S_COMPILE);


/***************************************************************************
  Builtin read only variables -- keep these together
    READONLY(NAME, CELLFUNC, FLAGS, LINK)
 ***************************************************************************/

READONLY (U0,           (cell)mem_get_start(),  0, const_UCELL_MAX);
READONLY (USIZE,        mem_get_ncells(),       0, readonly_U0);
READONLY (DOCOLMODE,    docolon_mode,           0, readonly_USIZE);
READONLY (STATE,        interpreter_state,      0, readonly_DOCOLMODE);


/***************************************************************************
  Builtin code words -- keep these together
    PRIMITIVE(NAME, FLAGS, CNAME, LINK)
 ***************************************************************************/

// ( a -- )
PRIMITIVE ("DROP", 0, _DROP, readonly_STATE) {
    REG(a);
    PPOP(a);
}


// ( b a -- a b )
PRIMITIVE ("SWAP", 0, _SWAP, _DROP) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    PPUSH(a);
    PPUSH(b);
}


// ( a - a a )
PRIMITIVE ("DUP", 0, _DUP, _SWAP) {
    REG(a);
    
    PTOP(a);
    PPUSH(a);
}


// ( b a -- a b )
PRIMITIVE ("OVER", 0, _OVER, _DUP) {
    REG(a);
    REG(b);

    PPOP(a);
    PTOP(b);
    PPUSH(a);
    PPUSH(b);
}


// ( c b a -- a c b )
PRIMITIVE ("ROT", 0, _ROT, _OVER) {
    REG(a);
    REG(b);
    REG(c);

    PPOP(a);
    PPOP(b);
    PPOP(c);
    PPUSH(a);
    PPUSH(c);
    PPUSH(b);
}


// ( c b a -- b a c )
PRIMITIVE ("-ROT", 0, _negROT, _ROT) {
    REG(a);
    REG(b);
    REG(c);

    PPOP(a);
    PPOP(b);
    PPOP(c);
    PPUSH(b);
    PPUSH(a);
    PPUSH(c);
}


// ( b a -- )
PRIMITIVE ("2DROP", 0, _2DROP, _negROT) {
    REG(a);
    PPOP(a);
    PPOP(a);
}


// ( b a -- b a b a )
PRIMITIVE ("2DUP", 0, _2DUP, _2DROP) {
    REG(a);
    REG(b);

    PPOP(a);
    PTOP(b);
    PPUSH(a);
    PPUSH(b);
    PPUSH(a);
}


// ( n*a n -- n*a n*a )
PRIMITIVE ("NDUP", 0, _NDUP, _2DUP) {
    cell *buf;
    REG(n);

    PPOP(n);
    if ((buf = malloc(n * sizeof(cell))) != NULL) {
        for (register int i=0; i<n; i++)  PPOP(buf[i]);
        for (register int i=0; i<n; i++)  PPUSH(buf[i]);
        for (register int i=0; i<n; i++)  PPUSH(buf[i]);
        free(buf);
    }
    else {
        fprintf(stderr, "malloc for 2 * %"PRIiPTR" cells failed in NDUP\n", n);
        _ABORT(NULL);
    }
}


// ( d c b a -- b a d c ) 
PRIMITIVE ("2SWAP", 0, _2SWAP, _NDUP) {
    REG(a);
    REG(b);
    REG(c);
    REG(d);

    PPOP(a);
    PPOP(b);
    PPOP(c);
    PPOP(d);

    PPUSH(b);
    PPUSH(a);
    PPUSH(d);
    PPUSH(c);
}


// ( a -- a ) or ( a -- a a )
PRIMITIVE ("?DUP", 0, _qDUP, _2SWAP) {
    REG(a);

    PTOP(a);
    if (a)  PPUSH(a);
}


// ( a -- a+1 )
PRIMITIVE ("1+", 0, _1plus, _qDUP) {
    REG(a);

    PPOP(a);
    PPUSH(a + 1);
}


// ( a -- a-1 )
PRIMITIVE ("1-", 0, _1minus, _1plus) {
    REG(a);
    
    PPOP(a);
    PPUSH(a - 1);
}


// ( a -- a+4 )
PRIMITIVE ("4+", 0, _4plus, _1minus) {
    REG(a);

    PPOP(a);
    PPUSH(a + 4);
}


// ( a -- a-4 )
PRIMITIVE ("4-", 0, _4minus, _4plus) {
    REG(a);

    PPOP(a);
    PPUSH(a - 4);
}


// ( b a -- a+b )
PRIMITIVE ("+", 0, _plus, _4minus) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    PPUSH(a + b);
}


// ( b a -- b - a )
PRIMITIVE ("-", 0, _minus, _plus) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    PPUSH(b - a);
}


// ( b a -- a * b )
PRIMITIVE ("*", 0, _multiply, _minus) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    PPUSH(a * b);
}


// ( b a -- b / a)
PRIMITIVE ("/", 0, _divide, _multiply) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    PPUSH(b / a);
}


// ( b a -- b % a)
PRIMITIVE ("MOD", 0, _modulus, _divide) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    PPUSH(b % a);
}


// ( b a -- a == b )
PRIMITIVE ("=", 0, _equals, _modulus) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    PPUSH(a == b);
}


// ( b a -- a != b )
PRIMITIVE ("<>", 0, _notequals, _equals) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    PPUSH(a != b);
}


// ( b a -- b < a )
PRIMITIVE ("<", 0, _lt, _notequals) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    PPUSH(b < a);
}


// ( b a -- b > a )
PRIMITIVE (">", 0, _gt, _lt) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    PPUSH(b > a);
}


// ( b a -- b <= a )
PRIMITIVE ("<=", 0, _lte, _gt) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    PPUSH(b <= a);
}


// ( b a -- b >= a )
PRIMITIVE (">=", 0, _gte, _lte) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    PPUSH(b >= a);
}


// ( a -- a == 0 )
PRIMITIVE ("0=", 0, _zero_equals, _gte) {
    REG(a);

    PPOP(a);
    PPUSH(a == 0);
}


// ( a -- a != 0 )
PRIMITIVE ("0<>", 0, _notzero_equals, _zero_equals) {
    REG(a);

    PPOP(a);
    PPUSH(a != 0);
}


// ( a -- a < 0 )
PRIMITIVE ("0<", 0, _zero_lt, _notzero_equals) {
    REG(a);

    PPOP(a);
    PPUSH(a < 0);
}


// ( a -- a > 0 )
PRIMITIVE ("0>", 0, _zero_gt, _zero_lt) {
    REG(a);

    PPOP(a);
    PPUSH(a > 0);
}


// ( a -- a <= 0 )
PRIMITIVE ("0<=", 0, _zero_lte, _zero_gt) {
    REG(a);

    PPOP(a);
    PPUSH(a <= 0);
}


// ( a -- a >= 0 )
PRIMITIVE ("0>=", 0, _zero_gte, _zero_lte) {
    REG(a);

    PPOP(a);
    PPUSH(a >= 0);
}


// ( b a -- a & b )
PRIMITIVE ("AND", 0, _AND, _zero_gte) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    PPUSH(a & b);
}


// ( b a -- a | b )
PRIMITIVE ("OR", 0, _OR, _AND) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    PPUSH(a | b);
}


// ( b a -- a ^ b )
PRIMITIVE ("XOR", 0, _XOR, _OR) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    PPUSH(a ^ b);
}


// ( a -- ~a )
PRIMITIVE ("INVERT", 0, _INVERT, _XOR) {
    REG(a);

    PPOP(a);
    PPUSH(~a);
}


/* Memory access primitives */


// ( value addr -- )
PRIMITIVE ("!", 0, _store, _INVERT) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);

    *((cell *) a) = b;
}


// ( addr -- value )
PRIMITIVE ("@", 0, _fetch, _store) {
    REG(a);
    REG(b);

    PPOP(a);
    b = *((cell *) a);
    PPUSH(b);
}


// ( delta addr -- )
PRIMITIVE ("+!", 0, _addstore, _fetch) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    *((cell*) a) += b;
}


// ( delta addr -- )
PRIMITIVE ("-!", 0, _substore, _addstore) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    *((cell*) a) -= b;
}


// ( value addr -- )
PRIMITIVE ("C!", 0, _storebyte, _substore) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(b);
    *((char*) a) = (char) (b & 0xFF);
}


// ( addr -- value )
PRIMITIVE ("C@", 0, _fetchbyte, _storebyte) {
    REG(a);
    REG(b);

    PPOP(a);
    b = *((char*) a);
    PPUSH(b);
}


// ( src dest -- src+1 dest+1 )
PRIMITIVE ("C@C!", 0, _ccopy, _fetchbyte) {
    REG(src);
    REG(dest);

    PPOP(dest);
    PPOP(src);
    *(char *) dest = *(const char *) src;
    PPUSH(src+1);
    PPUSH(dest+1);
}


// ( src dest len -- )
PRIMITIVE ("CMOVE", 0, _cmove, _ccopy) {
    REG(a);
    REG(b);
    REG(c);

    PPOP(a);  // len
    PPOP(b);  // dest
    PPOP(c);  // src

    // memmove(dest, src, len);
    memmove((void*) b, (void*) c, a);
}


/* Return stack primitives */

//A program shall not access values on the return stack (using R@, R>, 2R@ or 2R>) that it did 
//not place there using >R or 2>R;

// ( a -- ) ( R: -- a )
PRIMITIVE (">R", F_NOINTERP, _ltR, _cmove) {
    REG(a);

    PPOP(a);
    RPUSH(a);
}


// ( a b -- ) ( R: -- a b )
PRIMITIVE ("2>R", F_NOINTERP, _2ltR, _ltR) {
    REG(a);
    REG(b);

    PPOP(b);
    PPOP(a);
    RPUSH(a);
    RPUSH(b);
}


// ( -- a ) ( R: a -- )
PRIMITIVE ("R>", F_NOINTERP, _Rgt, _2ltR) {
    REG(a);

    RPOP(a);
    PPUSH(a);
}


// ( -- a b ) ( R: a b -- )
PRIMITIVE ("2R>", F_NOINTERP, _2Rgt, _Rgt) {
    REG(a);
    REG(b);

    RPOP(b);
    RPOP(a);
    PPUSH(a);
    PPUSH(b);
}


// ( -- a ) ( R: a -- a )
PRIMITIVE ("R@", F_NOINTERP, _Rat, _2Rgt) {
    REG(a);

    RTOP(a);
    PPUSH(a);
}


// ( -- a b ) ( R: a b -- a b )
PRIMITIVE ("2R@", F_NOINTERP, _2Rat, _Rat) {
    REG(a);
    REG(b);

    RPOP(b);
    RTOP(a);
    RPUSH(b);
    PPUSH(a);
    PPUSH(b);
}


/* Debug stuff */


// ( n*a n*b n -- n*a )
PRIMITIVE ("ASSERT",  0, _ASSERT, _2Rat) {
    int err = 0;
    new_cell *buf;
    register new_cell n;

    PPOP(n.u);

    if (stack_count(&parameter_stack) >= 2 * n.u) {
        if ((buf = malloc (2 * n.u * sizeof(cell))) != NULL) {
            for (int i = 0; i < 2 * n.u; i++) {
                PPOP(buf[i].u);
            }
            
            for (int i = 0; i < n.u; i++) {
                if (buf[i].u != buf[i + n.u].u)  err++;   
            }

            if (err) {
                printf("ASSERT %"PRIuPTR" failed:\nExpected: ", n.u);
                for (int i = n.u - 1; i >= 0; i--) {
                    printf("%"PRIuPTR" ", buf[i].u);
                }
                printf("\nFound: ");
                for (int i = 2*n.u - 1; i >= n.u; i--) {
                    printf ("%"PRIuPTR" ", buf[i].u);
                }
                printf("\n");
                _ABORT(NULL);
            }
            else {
                printf("ASSERT passed\n");
                for (int i = 2 * n.u - 1; i >= n.u; i--) {
                    PPUSH(buf[i].u);
                }
            }

            free (buf);
        }
        else {
            // Couldn't allocate memory?!
            fprintf(stderr, "malloc for %"PRIuPTR" cells failed in ASSERT\n", 2 * n.u);
            _ABORT(NULL);
        }
    }
    else {
        // stack would underflow!
        fprintf(stderr, "ASSERT: Not enough elements on stack "
                        "(requested: 2 * %"PRIuPTR", found %"PRIuPTR")\n",
                        n.u, stack_count(&parameter_stack));
        _ABORT(NULL);
    }
}


/* Other stuff */

// ( -- char )
PRIMITIVE ("KEY", 0, _KEY, _ASSERT) {
    REG(a);

    /* stdio is line buffered when attached to terminals :) */
    if ((a = fgetc(stdin)) != EOF) {
        PPUSH(a);
    }
    else if (feof(stdin))  exit(0);
    else  exit(1);
}


// ( char -- )
PRIMITIVE ("EMIT", 0, _EMIT, _KEY) {
    REG(a);

    PPOP(a);
    fputc(a, stdout);
}


// ( delim -- c-addr len )
PRIMITIVE ("WORD", 0, _WORD, _EMIT) {
    static int usebuf = 0;  // Double-buffered
    static CountedString buf[2];
    
    REG(delim);
    REG(key);
    REG(i);
    REG(blflag);

    memset(buf[usebuf].value, 0, sizeof(buf[usebuf].value));

    /* Get the delimiter */
    PPOP(delim);
    blflag = (delim == ' ');  // Treat control chars as whitespace when delim is space

    /* First skip leading delimiters */
    do {
        _KEY(NULL);
        PPOP(key);
        if (blflag && key < ' ')  key = delim;
    } while (key == delim);

    /* Then start storing chars in the buffer */
    i = 0;
    do {
        buf[usebuf].value[i++] = key;
        _KEY(NULL);
        PPOP(key);
        if (blflag && key < ' ')  key = delim;
    } while (i < MAX_COUNTED_STRING_LENGTH && key != delim);

    /* Return address and length on the stack */
    if (i < MAX_COUNTED_STRING_LENGTH) {
        buf[usebuf].length = i;
        PPUSH((cell) buf[usebuf].value);
        PPUSH(i);
        usebuf = (usebuf == 0 ? 1 : 0);
    }
    else {
        // Ran out of room, return some kind of error
        // Don't flip the buffers
        // FIXME anything else?
        PPUSH(0);
    }
}


// ( addr len -- value remainder )
PRIMITIVE ("NUMBER", 0, _NUMBER, _WORD) {
    char buf[MAX_WORD_LEN + 1];
    char *endptr;
    REG(a);
    REG(b);

    memset(buf, 0, MAX_WORD_LEN + 1);

    PPOP(a);  // len
    PPOP(b);  // addr

    memcpy(buf, (const char *) b, a);

    a = strtol(buf, &endptr, *var_BASE);    // value parsed
    b = strlen(buf) - (endptr - buf);       // number of chars left unparsed

    PPUSH(a);
    PPUSH(b);
}


// ( u n -- )
PRIMITIVE ("U.R", 0, _UdotR, _NUMBER) {
//    REG(base);
//    REG(a);
    register new_cell base;
    register new_cell a;
    REG(i);
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char buf[33];  // 32 digits, terminating '\0'
    int minlen;

    base.u = ((*var_BASE && *var_BASE <= 36 && *var_BASE >= 2) ? *var_BASE : 10);

    memset(buf, 0, sizeof(buf));
    i = 32; // Start at end of string
    PPOP(minlen);
    PPOP(a.u);
    do {
        buf[--i] = charset[a.u % base.u];
        a.u /= base.u;
    } while (a.u);
    // buf[i] is the start of the converted string
    printf("%*.32s", minlen, &buf[i]);
}


// ( i n -- )
PRIMITIVE (".R", 0, _dotR, _UdotR) {
    REG(base);
    REG(a);
    REG(i);
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char buf[34];  // possible sign, 32 digits, terminating '\0'
    int neg;
    int minlen;

    base = ((*var_BASE && *var_BASE <= 36 && *var_BASE >= 2) ? *var_BASE : 10);

    memset(buf, 0, sizeof(buf));
    i = 33; // Start at end of string
    PPOP(minlen);
    PPOP(a);
    neg = (a < 0);
    a = abs(a);  // FIXME when a == INTPTR_MIN, abs() will not work
    do {
        buf[--i] = charset[a % base];
        a /= base;
    } while (a);
    if (neg)  buf[--i] = '-';
    // buf[i] is the start of the converted string
    printf("%*.33s", minlen, &buf[i]);
}


// ( addr len -- addr )
PRIMITIVE ("FIND", 0, _FIND, _dotR) {
    char *word;
    size_t len;
    REG(a);

    PPOP(len);
    PPOP(a);
    word = (char *) a;

    DictEntry *de = (DictEntry *) *var_LATEST;
    DictEntry *result = NULL;

    while (de != &_dict___ROOT) {
        if (len == (de->name_length & (F_HIDDEN | F_LENMASK))) { // tricky - ignores hidden words
            if (strncmp(word, de->name, len) == 0) {
                // found a match
                result = de;
                break;
            }
        }
        de = de->link;
    }

    PPUSH((cell) result);
}


// ( addr -- cfa )
PRIMITIVE (">CFA", 0, _toCFA, _FIND) {
    REG(a);

    PPOP(a);
    PPUSH((cell) DE_to_CFA(a));
}


// ( addr -- dfa )
PRIMITIVE (">DFA", 0, _toDFA, _toCFA) {
    REG(a);

    PPOP(a);
    PPUSH((cell) DE_to_DFA(a));
}


// ( -- )
PRIMITIVE ("LIT", 0, _LIT, _toDFA) {
    docolon_mode = DM_LITERAL;
}


// ( addr len -- )
PRIMITIVE ("CREATE", 0, _CREATE, _LIT) {
    cell name_length;
    cell name_addr;
    DictHeader *new_header;

    PPOP(name_length);
    PPOP(name_addr);
    
    // clip the length to within range
    name_length = ((name_length > MAX_WORD_LEN) ? MAX_WORD_LEN : name_length);

    // set up the new entry
    new_header = *(DictHeader **)var_HERE;
    memset(new_header, 0, sizeof(DictHeader));
    new_header->link = *(DictEntry**)var_LATEST;
    new_header->name_length = name_length;
    strncpy(new_header->name, (const char *) name_addr, name_length);
    new_header->sentinel = SENTINEL;

    // update HERE and LATEST
    *var_LATEST = (cell) new_header;
    *var_HERE = (cell) new_header + sizeof(DictHeader);
}


// ( a -- )
PRIMITIVE (",", 0, _comma, _CREATE) {
    REG(a);

    PPOP(a);
    **(cell**)var_HERE = a;
    *var_HERE += sizeof(cell);
    // FIXME is this right?
}


// ( -- )
PRIMITIVE ("[", F_IMMED, _lbrac, _comma) {
    interpreter_state = S_INTERPRET;
}


// ( -- )
PRIMITIVE ("]", 0, _rbrac, _lbrac) {
    interpreter_state = S_COMPILE;
}


// ( -- )
PRIMITIVE (":", 0, _colon, _rbrac) {
    PPUSH(' ');
    _WORD(NULL);
    _CREATE(NULL);
    PPUSH(*const_DOCOL);
    _comma(NULL);
    PPUSH(*var_LATEST);
    _HIDDEN(NULL);
    _rbrac(NULL);
}


// ( -- )
PRIMITIVE (";", F_IMMED | F_NOINTERP, _semicolon, _colon) {
    PPUSH(*const_EXIT); // FIXME i think this is wrong
    _comma(NULL);
    PPUSH(*var_LATEST);
    _HIDDEN(NULL);
    _lbrac(NULL);
}


// ( -- )
PRIMITIVE ("IMMEDIATE", F_IMMED, _IMMEDIATE, _semicolon) {
    DictEntry *latest = *(DictEntry **)var_LATEST;

    latest->name_length ^= F_IMMED;
}


// ( -- )
PRIMITIVE ("NOINTERPRET", F_IMMED, _NOINTERP, _IMMEDIATE) {
    DictEntry *latest = *(DictEntry **)var_LATEST;
    
    latest->name_length ^= F_NOINTERP;
}


// ( addr -- )
PRIMITIVE ("HIDDEN", 0, _HIDDEN, _NOINTERP) {
    REG(a);

    PPOP(a);

    ((DictEntry*) a)->name_length ^= F_HIDDEN;
}


// ( -- )
PRIMITIVE ("HIDE", 0, _HIDE, _HIDDEN) {
    PPUSH(' ');
    _WORD(NULL);
    _FIND(NULL);
    _HIDDEN(NULL);
}


// ( -- )
PRIMITIVE ("'", 0, _tick, _HIDE) {
    PPUSH(' ');
    _WORD(NULL);
    _FIND(NULL);
    _toCFA(NULL);
}


// ( -- )
PRIMITIVE ("BRANCH", 0, _BRANCH, _tick) {
    docolon_mode = DM_BRANCH;
}


// ( cond -- )
PRIMITIVE ("0BRANCH", 0, _0BRANCH, _BRANCH) {
    REG(a);

    PPOP(a);
    if (a == 0)  docolon_mode = DM_BRANCH;
    else         docolon_mode = DM_SKIP;
}


// ( -- )
PRIMITIVE ("LITSTRING", 0, _LITSTRING, _0BRANCH) {
    docolon_mode = DM_SLITERAL;
}


// ( -- )
PRIMITIVE ("TELL", 0, _TELL, _LITSTRING) {
    REG(a);
    REG(b);

    PPOP(a);  // len
    PPOP(b);  // addr
    while (a--) {
        putchar(*(char *)(b++));
    }
}


// ( -- status )
PRIMITIVE ("UGROW", 0, _UGROW, _TELL) {
    REG(a);

    a = mem_grow(*var_UINCR);
    PPUSH(a);
}


// ( ncells -- status )
PRIMITIVE ("UGROWN", 0, _UGROWN, _UGROW) {
    REG(a);

    PPOP(a);
    a = mem_grow(a);
    PPUSH(a);
}

// ( ncells -- status )
PRIMITIVE ("USHRINK", 0, _USHRINK, _UGROWN) {
    REG(a);

    PPOP(a);
    a = mem_shrink(a);
    PPUSH(a);
}


// ( -- )
PRIMITIVE ("QUIT", 0, _QUIT, _USHRINK) {
    longjmp(warm_boot, 1);
}


// ( -- )
PRIMITIVE ("ABORT", 0, _ABORT, _QUIT) {
    fprintf(stderr, "Abort called, rebooting\n");
    longjmp(cold_boot, 1);
}


// ( -- )
PRIMITIVE ("breakpoint", F_IMMED, _breakpoint, _ABORT) {
    REG(a);

    PPUSH(1);
    PPOP(a);
}


// ( a -- a * sizeof(cell))
PRIMITIVE ("CELLS", 0, _CELLS, _breakpoint) {
    REG(a);

    PPOP(a);
    PPUSH(a * sizeof(cell));
}


// (a -- a / sizeof(cell))
PRIMITIVE ("/CELLS", 0, _divCELLS, _CELLS) {
    REG(a);
    REG(b);

    PPOP(a);
    b = sizeof(cell);  // wtf, why do I need to do it this way?
    PPUSH(a / b);
}


// ( xt -- )
PRIMITIVE ("EXECUTE", 0, _EXECUTE, _divCELLS) {
    REG(xt);

    PPOP(xt);
    do_execute((pvf*) xt, (cell*) xt + 1);
}



/***************************************************************************
    The LATEST variable denotes the top of the dictionary.  Its initial
    value points to its own dictionary entry (tricky).
    IMPORTANT:
    * Be sure to update its link pointer if you add more builtins before it!
    * This must be the LAST entry added to the dictionary!
 ***************************************************************************/
VARIABLE (LATEST, (cell) &_dict_var_LATEST, 0, _EXECUTE);  // FIXME keep this updated!
