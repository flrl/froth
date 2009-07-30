#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "exception.h"
#include "forth.h"
#include "stack.h"
#include "vm.h"

// Function signature for a primitive
#define DECLARE_PRIMITIVE(P)    void P(void *pfa)

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
            SENTINEL, do_variable, {{INITIAL}} };                   \
    cell * const var_##NAME = &_dict_var_##NAME.param[0]

// Define a constant and add it to the dictionary; also create a pointer for direct access
#define CONSTANT(NAME, VALUE, FLAGS, LINK)                          \
    DictEntry _dict_const_##NAME =                                  \
        { &_dict_##LINK, ((FLAGS) | (sizeof(#NAME) - 1)), #NAME,    \
            SENTINEL, do_constant, {{VALUE}} };                     \
    const cell * const const_##NAME = &_dict_const_##NAME.param[0]

// Define a "read only" variable and add it to the dictionary (special case of PRIMITIVE)
#define READONLY(NAME, CELLFUNC, FLAGS, LINK)                       \
    DECLARE_PRIMITIVE(readonly_##NAME);                             \
    DictEntry _dict_readonly_##NAME =                               \
        { &_dict_##LINK, ((FLAGS) | (sizeof(#NAME) - 1)), #NAME,    \
            SENTINEL, readonly_##NAME, };                           \
    DECLARE_PRIMITIVE(readonly_##NAME) { REG(a); a = (CELLFUNC); DPUSH(a); }

// Shorthand macros to make repetitive code more writeable, but possibly less readable
#define REG(X)          register cell X


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
CONSTANT (DOCOL,        (intptr_t)&do_colon,    0,  const_VERSION);
CONSTANT (DOVAR,        (intptr_t)&do_variable, 0,  const_DOCOL);
CONSTANT (DOCON,        (intptr_t)&do_constant, 0,  const_DOVAR);
CONSTANT (DOVAL,        (intptr_t)&do_value,    0,  const_DOCON);
CONSTANT (EXIT,         0,                      0,  const_DOVAL);
CONSTANT (F_IMMED,      F_IMMED,                0,  const_EXIT);
CONSTANT (F_COMPONLY,   F_COMPONLY,             0,  const_F_IMMED);
CONSTANT (F_HIDDEN,     F_HIDDEN,               0,  const_F_COMPONLY);
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

READONLY (U0,           (cell)mem_get_start(),              0, const_UCELL_MAX);
READONLY (USIZE,        (cell)(uintptr_t)mem_get_ncells(),  0, readonly_U0);
READONLY (DOCOLMODE,    (cell)(intptr_t)docolon_mode,       0, readonly_USIZE);
READONLY (STATE,        (cell)(intptr_t)interpreter_state,  0, readonly_DOCOLMODE);


/***************************************************************************
  Builtin code words -- keep these together
    PRIMITIVE(NAME, FLAGS, CNAME, LINK)
 ***************************************************************************/

// ( a -- )
PRIMITIVE ("DROP", 0, _DROP, readonly_STATE) {
    REG(a);
    DPOP(a);
}


// ( b a -- a b )
PRIMITIVE ("SWAP", 0, _SWAP, _DROP) {
    REG(a);
    REG(b);

    DPOP(a);
    DPOP(b);
    DPUSH(a);
    DPUSH(b);
}


// ( a - a a )
PRIMITIVE ("DUP", 0, _DUP, _SWAP) {
    REG(a);
    
    DPEEK(a);
    DPUSH(a);
}


// ( b a -- b a b )
PRIMITIVE ("OVER", 0, _OVER, _DUP) {
    REG(a);
    REG(b);

    DPOP(a);
    DPEEK(b);
    DPUSH(a);
    DPUSH(b);
}


// ( b a -- a b a )
PRIMITIVE ("TUCK", 0, _TUCK, _OVER) {
    REG(a);
    REG(b);

    DPOP(a);
    DPOP(b);
    DPUSH(a);
    DPUSH(b);
    DPUSH(a);
}


// ( xu xu-1 .. x1 x0 u -- xu xu-1 .. x1 x0 xu )
PRIMITIVE ("PICK", 0, _PICK, _TUCK) {
    REG(u);

    DPOP(u);
    DPICK(u.as_u);
}


// ( xu xu-1 .. x1 x0 u -- xu-1 .. x1 x0 xu )
PRIMITIVE ("ROLL", 0, _ROLL, _PICK) {
    REG(u);

    DPOP(u);
    DROLL(u.as_u);
}


// ( c b a -- a c b )
PRIMITIVE ("ROT", 0, _ROT, _ROLL) {
    REG(a);
    REG(b);
    REG(c);

    DPOP(a);
    DPOP(b);
    DPOP(c);
    DPUSH(a);
    DPUSH(c);
    DPUSH(b);
}


// ( c b a -- b a c )
PRIMITIVE ("-ROT", 0, _negROT, _ROT) {
    REG(a);
    REG(b);
    REG(c);

    DPOP(a);
    DPOP(b);
    DPOP(c);
    DPUSH(b);
    DPUSH(a);
    DPUSH(c);
}


// ( b a -- )
PRIMITIVE ("2DROP", 0, _2DROP, _negROT) {
    REG(a);
    DPOP(a);
    DPOP(a);
}


// ( n*a n -- )
PRIMITIVE ("NDROP", 0, _NDROP, _2DROP) {
    register intptr_t n;

    n = stack_pop(&data_stack).as_i;
    n = data_stack.top - n;
    if (n < -1)  n = -1;
    data_stack.top = n;
}


// ( b a -- b a b a )
PRIMITIVE ("2DUP", 0, _2DUP, _NDROP) {
    REG(a);
    REG(b);

    DPOP(a);
    DPEEK(b);
    DPUSH(a);
    DPUSH(b);
    DPUSH(a);
}


// ( n*a n -- n*a n*a )
PRIMITIVE ("NDUP", 0, _NDUP, _2DUP) {  // FIXME this implementation sucks
    cell *buf;
    register uintptr_t n;

    n = stack_pop(&data_stack).as_u;

    if ((buf = malloc(n * sizeof(cell))) != NULL) {
        for (register int i=0; i<n; i++)  DPOP(buf[i]);
        for (register int i=n-1; i>=0; i--)  DPUSH(buf[i]);
        for (register int i=n-1; i>=0; i--)  DPUSH(buf[i]);
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

    DPOP(a);
    DPOP(b);
    DPOP(c);
    DPOP(d);

    DPUSH(b);
    DPUSH(a);
    DPUSH(d);
    DPUSH(c);
}


// ( a -- 0 | a a )
PRIMITIVE ("?DUP", 0, _qDUP, _2SWAP) {
    REG(a);

    DPEEK(a);
    if (a.as_i)  DPUSH(a);
}


// ( a -- a + 1 )
PRIMITIVE ("1+", 0, _1plus, _qDUP) {
    REG(a);

    DPOP(a);
    DPUSH((cell) (a.as_i + 1));
}


// ( a -- a - 1 )
PRIMITIVE ("1-", 0, _1minus, _1plus) {
    REG(a);
    
    DPOP(a);
    DPUSH((cell) (a.as_i - 1));
}


// ( a -- a + 4 )
PRIMITIVE ("4+", 0, _4plus, _1minus) {
    REG(a);

    DPOP(a);
    DPUSH((cell) (a.as_i + 4));
}


// ( a -- a - 4 )
PRIMITIVE ("4-", 0, _4minus, _4plus) {
    REG(a);

    DPOP(a);
    DPUSH((cell) (a.as_i - 4));
}


// ( a b -- a + b )
PRIMITIVE ("+", 0, _plus, _4minus) {
    REG(a);
    REG(b);

    DPOP(b);
    DPOP(a);
    DPUSH((cell) (a.as_i + b.as_i));
}


// ( a b -- a - b )
PRIMITIVE ("-", 0, _minus, _plus) {
    REG(a);
    REG(b);

    DPOP(b);
    DPOP(a);
    DPUSH((cell) (a.as_i - b.as_i));
}


// ( a b -- a * b )
PRIMITIVE ("*", 0, _multiply, _minus) {
    REG(a);
    REG(b);

    DPOP(b);
    DPOP(a);
    DPUSH((cell) (a.as_i * b.as_i));
}


// ( a b -- a / b)
PRIMITIVE ("/", 0, _divide, _multiply) {
    REG(a);
    REG(b);

    DPOP(b);
    DPOP(a);
    DPUSH((cell) (a.as_i / b.as_i));
}


// ( a b -- a % b)
PRIMITIVE ("MOD", 0, _modulus, _divide) {
    REG(a);
    REG(b);

    DPOP(b);
    DPOP(a);
    DPUSH((cell) (a.as_i % b.as_i));
}


// ( a b -- a == b )
PRIMITIVE ("=", 0, _equals, _modulus) {
    REG(a);
    REG(b);

    DPOP(b);
    DPOP(a);
    DPUSH((cell)(intptr_t) (a.as_i == b.as_i));
}


// ( a b -- a != b )
PRIMITIVE ("<>", 0, _notequals, _equals) {
    REG(a);
    REG(b);

    DPOP(b);
    DPOP(a);
    DPUSH((cell)(intptr_t) (a.as_i != b.as_i));
}


// ( a b -- a < b )
PRIMITIVE ("<", 0, _lt, _notequals) {
    REG(a);
    REG(b);

    DPOP(b);
    DPOP(a);
    DPUSH((cell)(intptr_t) (a.as_i < b.as_i));
}


// ( a b -- a > b )
PRIMITIVE (">", 0, _gt, _lt) {
    REG(a);
    REG(b);

    DPOP(b);
    DPOP(a);
    DPUSH((cell)(intptr_t) (a.as_i > b.as_i));
}


// ( a b -- a <= b )
PRIMITIVE ("<=", 0, _lte, _gt) {
    REG(a);
    REG(b);

    DPOP(b);
    DPOP(a);
    DPUSH((cell)(intptr_t) (a.as_i <= b.as_i));
}


// ( a b -- a >= b )
PRIMITIVE (">=", 0, _gte, _lte) {
    REG(a);
    REG(b);

    DPOP(b);
    DPOP(a);
    DPUSH((cell)(intptr_t) (a.as_i >= b.as_i));
}


// ( a -- a == 0 )
PRIMITIVE ("0=", 0, _zero_equals, _gte) {
    REG(a);

    DPOP(a);
    DPUSH((cell)(intptr_t) (a.as_i == 0));
}


// ( a -- a != 0 )
PRIMITIVE ("0<>", 0, _notzero_equals, _zero_equals) {
    REG(a);

    DPOP(a);
    DPUSH((cell)(intptr_t) (a.as_i != 0));
}


// ( a -- a < 0 )
PRIMITIVE ("0<", 0, _zero_lt, _notzero_equals) {
    REG(a);

    DPOP(a);
    DPUSH((cell)(intptr_t) (a.as_i < 0));
}


// ( a -- a > 0 )
PRIMITIVE ("0>", 0, _zero_gt, _zero_lt) {
    REG(a);

    DPOP(a);
    DPUSH((cell)(intptr_t) (a.as_i > 0));
}


// ( a -- a <= 0 )
PRIMITIVE ("0<=", 0, _zero_lte, _zero_gt) {
    REG(a);

    DPOP(a);
    DPUSH((cell)(intptr_t) (a.as_i <= 0));
}


// ( a -- a >= 0 )
PRIMITIVE ("0>=", 0, _zero_gte, _zero_lte) {
    REG(a);

    DPOP(a);
    DPUSH((cell)(intptr_t) (a.as_i >= 0));
}


// ( a b -- a & b )
PRIMITIVE ("AND", 0, _AND, _zero_gte) {
    REG(a);
    REG(b);

    DPOP(b);
    DPOP(a);
    DPUSH((cell) (a.as_u & b.as_u));
}


// ( a b -- a | b )
PRIMITIVE ("OR", 0, _OR, _AND) {
    REG(a);
    REG(b);

    DPOP(b);
    DPOP(a);
    DPUSH((cell) (a.as_u | b.as_u));
}


// ( a b -- a ^ b )
PRIMITIVE ("XOR", 0, _XOR, _OR) {
    REG(a);
    REG(b);

    DPOP(b);
    DPOP(a);
    DPUSH((cell) (a.as_u ^ b.as_u));
}


// ( a -- ~a )
PRIMITIVE ("INVERT", 0, _INVERT, _XOR) {
    REG(a);

    DPOP(a);
    DPUSH((cell) ~a.as_u);
}


/* Memory access primitives */


// ( a addr -- )
PRIMITIVE ("!", 0, _store, _INVERT) {
    REG(addr);
    REG(a);

    DPOP(addr);
    DPOP(a);

    *addr.as_dfa = a;
}


// ( addr -- a )
PRIMITIVE ("@", 0, _fetch, _store) {
    REG(addr);
    REG(a);

    DPOP(addr);
    a = *addr.as_dfa;
    DPUSH(a);
}


// ( delta addr -- )
PRIMITIVE ("+!", 0, _addstore, _fetch) {
    REG(a);
    REG(b);

    DPOP(a);
    DPOP(b);
    a.as_dfa->as_i += b.as_i;
}


// ( delta addr -- )
PRIMITIVE ("-!", 0, _substore, _addstore) {
    REG(a);
    REG(b);

    DPOP(a);
    DPOP(b);
    a.as_dfa->as_i -= b.as_i;
}


// ( value addr -- )
PRIMITIVE ("C!", 0, _storebyte, _substore) {
    REG(a);
    REG(b);

    DPOP(a);
    DPOP(b);
    *(char*) a.as_ptr = (char) b.as_i;
}


// ( addr -- value )
PRIMITIVE ("C@", 0, _fetchbyte, _storebyte) {
    REG(a);
    register uintptr_t b;

    DPOP(a);
    b = *(unsigned char*) a.as_ptr;
    DPUSH((cell) b);
}


// ( src dest -- src+1 dest+1 )
PRIMITIVE ("C@C!", 0, _ccopy, _fetchbyte) {
    REG(src);
    REG(dest);

    DPOP(dest);
    DPOP(src);
    *(char *) dest.as_ptr = *(const char *) src.as_ptr;
    DPUSH((cell) (src.as_u + 1));
    DPUSH((cell) (dest.as_u + 1));
}


// ( src dest len -- )
PRIMITIVE ("CMOVE", 0, _cmove, _ccopy) {
    REG(a);
    REG(b);
    REG(c);

    DPOP(a);  // len
    DPOP(b);  // dest
    DPOP(c);  // src

    // memmove(dest, src, len);
    memmove(b.as_ptr, c.as_ptr, a.as_u);
}


// ( n -- )
PRIMITIVE ("ALLOT", 0, _ALLOT, _cmove) {
    REG(n);

    DPOP(n);
    var_HERE->as_i += n.as_i;
}


/* Return stack primitives */

//A program shall not access values on the return stack (using R@, R>, 2R@ or 2R>) that it did 
//not place there using >R or 2>R;

// ( a -- ) ( R: -- a )
PRIMITIVE (">R", F_COMPONLY, _ltR, _ALLOT) {
    REG(a);

    DPOP(a);
    RPUSH(a);
}


// ( a b -- ) ( R: -- a b )
PRIMITIVE ("2>R", F_COMPONLY, _2ltR, _ltR) {
    REG(a);
    REG(b);

    DPOP(b);
    DPOP(a);
    RPUSH(a);
    RPUSH(b);
}


// ( -- a ) ( R: a -- )
PRIMITIVE ("R>", F_COMPONLY, _Rgt, _2ltR) {
    REG(a);

    RPOP(a);
    DPUSH(a);
}


// ( -- a b ) ( R: a b -- )
PRIMITIVE ("2R>", F_COMPONLY, _2Rgt, _Rgt) {
    REG(a);
    REG(b);

    RPOP(b);
    RPOP(a);
    DPUSH(a);
    DPUSH(b);
}


// ( -- a ) ( R: a -- a )
PRIMITIVE ("R@", F_COMPONLY, _Rat, _2Rgt) {
    REG(a);

    RPEEK(a);
    DPUSH(a);
}


// ( -- a b ) ( R: a b -- a b )
PRIMITIVE ("2R@", F_COMPONLY, _2Rat, _Rat) {
    REG(a);
    REG(b);

    RPOP(b);
    RPEEK(a);
    RPUSH(b);
    DPUSH(a);
    DPUSH(b);
}


// ( R: a -- a+1 )
PRIMITIVE ("R1+", F_COMPONLY, _R1plus, _2Rat) {
    if (stack_count(&return_stack)) {
        return_stack.values[return_stack.top].as_i ++;
    }
    // FIXME error handling
}


// ( R: a -- a-1 )
PRIMITIVE ("R1-", F_COMPONLY, _R1minus, _R1plus) {
    if (stack_count(&return_stack)) {
        return_stack.values[return_stack.top].as_i --;
    }
    // FIXME error handling
}


/* Control stack primitives */

// ( a -- ) ( C: -- a )
PRIMITIVE (">CTRL", F_COMPONLY, _gtCTRL, _R1minus) {
    REG(a);

    DPOP(a);
    CPUSH(a);
}


// ( a b -- ) ( C: -- a b )
PRIMITIVE ("2>CTRL", F_COMPONLY, _2gtCTRL, _gtCTRL) {
    REG(a);
    REG(b);

    DPOP(b);
    DPOP(a);
    CPUSH(a);
    CPUSH(b);
}


// ( -- a ) ( C: a -- )
PRIMITIVE ("CTRL>", F_COMPONLY, _CTRLgt, _2gtCTRL) {
    REG(a);

    CPOP(a);
    DPUSH(a);
}


// ( -- a b ) ( C: a b -- )
PRIMITIVE ("2CTRL>", F_COMPONLY, _2CTRLgt, _CTRLgt) {
    REG(a);
    REG(b);

    CPOP(b);
    CPOP(a);
    DPUSH(a);
    DPUSH(b);
}


// ( -- a ) ( C: a -- a )
PRIMITIVE ("CTRL@", F_COMPONLY, _CTRLat, _2CTRLgt) {
    REG(a);

    CPEEK(a);
    DPUSH(a);
}


// ( -- a b ) ( C: a b -- a b )
PRIMITIVE ("2CTRL@", F_COMPONLY, _2CTRLat, _CTRLat) {
    REG(a);
    REG(b);

    CPOP(b);
    CPEEK(a);
    CPUSH(b);
    DPUSH(a);
    DPUSH(b);
}


/* Debug stuff */

// ( n*a n*b n -- n*a )
PRIMITIVE ("ASSERT", 0, _ASSERT, _2CTRLat) {
    REG(a);
    register uintptr_t n;

    DPOP(a);
    n = a.as_u;
    // FIXME don't flip out on negative input

    if (stack_count(&data_stack) >= 2 * n) {
        cell *orig = &data_stack.values[1 + data_stack.top - 2 * n];
        cell *dup  = &data_stack.values[1 + data_stack.top - n];
        if (memcmp(orig, dup, n * sizeof(cell)) == 0) {
            puts("ASSERT passed");
        }
        else {
            printf("ASSERT %"PRIuPTR" failed:\n", n);
            int i;
            for (i=0; i < n; i++) {
                if (i % 8 == 0)  printf("expected> ");
                printf("%"PRIiPTR" ", dup[i].as_i);
                if (i % 8 == 7)  putchar('\n');
            }
            if (i % 8 != 0)  putchar('\n');
            for (i=0; i < n; i++) {
                if (i % 8 == 0)  printf("found>    ");
                printf("%"PRIiPTR" ", orig[i].as_i);
                if (i % 8 == 7)  putchar('\n');
            }
            if (i % 8 != 0)  putchar('\n');
        }
        data_stack.top -= n;
    }
    else {
        // stack would underflow!
        fprintf(stderr, "ASSERT: Not enough elements on stack "
                        "(expected %"PRIuPTR", found %"PRIuPTR")\n",
                        2 * n, stack_count(&data_stack));
        _ABORT(NULL);
        // FIXME throw
    }
}


// ( -- )
PRIMITIVE (".S", 0, _dotS, _ASSERT) {
    if (stack_count(&data_stack) == 0) {
        puts("(empty)");
    }
    else {
        register int i;
        for (i = 0; i < stack_count(&data_stack); i++) {
            if (i % 8 == 0)  printf("stack>  ");
            printf("%"PRIiPTR" ", data_stack.values[i].as_i);
            if (i % 8 == 7)  putchar('\n');
        }
        if (i % 8 != 0)  putchar('\n');  // if the last number didn't just print one out itself
    }
}


/* Other stuff */

// ( -- char )
PRIMITIVE ("KEY", 0, _KEY, _dotS) {
    REG(a);

    /* stdio is line buffered when attached to terminals :) */
    a = getkey();
    if (a.as_i != EOF) {
        DPUSH(a);
//        if (a == '\n')  puts(" ok");
    }
    else if (feof(stdin))  exit(0);
    else  exit(1);
}


// ( char -- )
PRIMITIVE ("EMIT", 0, _EMIT, _KEY) {
    REG(a);

    DPOP(a);
    fputc(a.as_i, stdout);
}


// ( delim -- c-addr )
PRIMITIVE ("WORD", 0, _WORD, _EMIT) {
    static int usebuf = 0;  // Double-buffered
    static CountedString buf[2];
    
    register int i;
    register int blflag;
    REG(delim);
    REG(key);

    memset(buf[usebuf].value, 0, sizeof(buf[usebuf].value));

    /* Get the delimiter */
    DPOP(delim);
    blflag = (delim.as_i == ' ');  // Treat control chars as whitespace when delim is space

    /* First skip leading delimiters */
    do {
        _KEY(NULL);
        DPOP(key);
        if (blflag && key.as_i < ' ')  key = delim;
    } while (key.as_i == delim.as_i);

    /* Then start storing chars in the buffer */
    i = 0;
    do {
        buf[usebuf].value[i++] = key.as_i;
        _KEY(NULL);
        DPOP(key);
        if (blflag && key.as_i < ' ')  key = delim;
    } while (i < MAX_COUNTED_STRING_LENGTH && key.as_i != delim.as_i);

    /* Return address of counted string on the stack */
    if (i < MAX_COUNTED_STRING_LENGTH) {
        buf[usebuf].length = i;
        DPUSH((cell)(uintptr_t) &buf[usebuf]);
        usebuf = (usebuf == 0 ? 1 : 0);
    }
    else {
        // Ran out of room
        // Don't flip the buffers
        throw(EXC_STR_OVER);  /* doesn't return */
    }
}


// ( c-addr -- n status )
PRIMITIVE ("NUMBER", 0, _NUMBER, _WORD) {
    CountedString *word;
    char *endptr;
    REG(a);

    // Eliminate invalid base early
    if (var_BASE->as_i != 0 && (var_BASE->as_i < 2 || var_BASE->as_i > 36)) {
        throw(EXC_INV_NUM);  /* doesn't return */
    }

    DPOP(a);
    word = a.as_cs;

    // Bail out if the word is junk
    if (word == NULL || word->length <= 0) {
        throw(EXC_INV_ADDR);  /* doesn't return */
    }

    errno = 0;
    a.as_u = strtoul(word->value, &endptr, var_BASE->as_i);
    if (errno == 0 || errno == EINVAL) {
        // Now if it's EINVAL it just means it wasn't a number or had trailing junk
        DPUSH(a);   // value
        a.as_i = word->value + word->length - endptr;
        DPUSH(a);   // number of chars left unparsed
    }
    else {
        // Some other error occurred
        perror("NUMBER");   // FIXME 
        throw(12);  /* doesn't return */ // FIXME magic number
    }
}


// ( u n -- )
PRIMITIVE ("U.R", 0, _UdotR, _NUMBER) {
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char buf[33];  // 32 digits, terminating '\0'
    int minlen;
    REG(a);
    register int i;
    register unsigned int base;

    base = ((var_BASE->as_u >= 2 && var_BASE->as_u <= 36) ? var_BASE->as_u : 10);

    memset(buf, 0, sizeof(buf));
    i = 32; // Start at end of string
    DPOP(a);
    minlen = a.as_u;
    DPOP(a);
    do {
        buf[--i] = charset[a.as_u % base];
        a.as_u /= base;
    } while (a.as_u);
    // buf[i] is the start of the converted string
    printf("%*.33s", minlen, &buf[i]);
}


// ( i n -- )
PRIMITIVE (".R", 0, _dotR, _UdotR) {
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char buf[34];  // possible sign, 32 digits, terminating '\0'
    int neg;
    int minlen;
    REG(a);
    register int i;
    register unsigned int base;

    base = ((var_BASE->as_u >= 2 && var_BASE->as_u <= 36) ? var_BASE->as_u : 10);

    memset(buf, 0, sizeof(buf));
    i = 33; // Start at end of string
    DPOP(a);
    minlen = a.as_u;
    DPOP(a);
    neg = (a.as_i < 0);
    a.as_i = abs(a.as_i);  // FIXME when a == INTPTR_MIN, abs() will not work
    do {
        buf[--i] = charset[a.as_i % base];
        a.as_i /= base;
    } while (a.as_i);
    if (neg)  buf[--i] = '-';
    // buf[i] is the start of the converted string
    printf("%*.33s", minlen, &buf[i]);
}


// ( c-addr -- addr )
PRIMITIVE ("FIND", 0, _FIND, _dotR) {
    REG(a);

    DPOP(a);
    CountedString *word = a.as_cs;

    DictEntry *de = var_LATEST->as_de;
    DictEntry *result = NULL;

    while (de != &_dict___ROOT) {
        // tricky - ignores hidden words
        if (word->length == (de->flags & (F_HIDDEN | F_LENMASK))) { 
            if (strncmp(word->value, de->name, word->length) == 0) {
                // found a match
                result = de;
                break;
            }
        }
        de = de->link;
    }

    DPUSH((cell) result);
}


// ( addr -- addr )
PRIMITIVE ("DE>CFA", 0, _DEtoCFA, _FIND) {
    REG(a);

    DPOP(a);
    DPUSH((cell)(uintptr_t) DE_to_CFA(a.as_de));
}


// ( addr -- addr )
PRIMITIVE ("DE>DFA", 0, _DEtoDFA, _DEtoCFA) {
    REG(a);

    DPOP(a);
    DPUSH((cell)(uintptr_t) DE_to_DFA(a.as_de));
}


// ( addr -- c-addr )
PRIMITIVE ("DE>NAME", 0, _DEtoNAME, _DEtoDFA) {
    REG(a);

    DPOP(a);
    DPUSH((cell)(uintptr_t) DE_to_NAME(a.as_de));
}


// ( addr -- addr )
PRIMITIVE ("DFA>DE", 0, _DFAtoDE, _DEtoNAME) {
    REG(a);

    DPOP(a);
    DPUSH((cell)(uintptr_t) DFA_to_DE(a.as_dfa));
}


// ( addr -- addr )
PRIMITIVE ("DFA>CFA", 0, _DFAtoCFA, _DFAtoDE) {
    REG(a);

    DPOP(a);
    DPUSH((cell)(uintptr_t) DFA_to_CFA(a.as_dfa));
}


// ( -- )
PRIMITIVE ("LIT", 0, _LIT, _DFAtoCFA) {
    docolon_mode = DM_LITERAL;
}


// ( -- addr )
PRIMITIVE ("CREATE", 0, _CREATE, _LIT) {
    DictHeader *new_header;
    REG(a);
    CountedString *name;

    // Parse a space-delimited name from input
    DPUSH((cell)(intptr_t) ' ');
    _WORD(NULL);
    DPOP(a);
    name = a.as_cs;

    if (name->length <= 0) {
        throw(EXC_EMPTY_NAME); /* doesn't return */
    }
    else if (name->length > MAX_WORD_LEN) {
        throw(EXC_NAMELEN);  /* doesn't return */
    }

    // Initialise a DictHeader for it
    a.as_u = CELLALIGN(var_HERE->as_u);
    new_header = (DictHeader *) a.as_ptr;
    memset(new_header, 0, sizeof(DictHeader));
    new_header->link = *(DictEntry **) var_LATEST;
    new_header->flags = name->length & F_LENMASK;
    strncpy(new_header->name, name->value, new_header->flags);
    new_header->sentinel = SENTINEL;
    new_header->code = &do_variable;

    // Update LATEST and HERE
    var_LATEST->as_de = (DictEntry*) new_header;
    var_HERE->as_dfa = DE_to_DFA(new_header);

    // Push DFA
    DPUSH(*var_HERE);
}


// ( a -- )
PRIMITIVE (",", 0, _comma, _CREATE) {
    REG(a);

    DPOP(a);
    **(cell**)var_HERE = a;
    var_HERE->as_ptr += sizeof(cell);
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
    REG(a);

    _CREATE(NULL);
    DPOP(a);
    a = (cell) DFA_to_CFA(a.as_dfa);
    *a.as_xt = const_DOCOL->as_pvf;
    DPUSH(*var_LATEST);
    _HIDDEN(NULL);
    _rbrac(NULL);
}


// ( -- )
PRIMITIVE (";", F_IMMED | F_COMPONLY, _semicolon, _colon) {
    DPUSH(*const_EXIT); // FIXME i think this is wrong. XXX ok it's mostly fine, just tricky.
    _comma(NULL);
    DPUSH(*var_LATEST);
    _HIDDEN(NULL);
    _lbrac(NULL);
}


// ( -- )
PRIMITIVE ("IMMEDIATE", F_IMMED, _IMMEDIATE, _semicolon) {
    DictEntry *latest = *(DictEntry **)var_LATEST;

    latest->flags ^= F_IMMED;
}


// ( -- )
PRIMITIVE ("COMPILE-ONLY", F_IMMED, _COMPILE_ONLY, _IMMEDIATE) {
    DictEntry *latest = *(DictEntry **)var_LATEST;
    
    latest->flags ^= F_COMPONLY;
}


// ( addr -- )
PRIMITIVE ("HIDDEN", 0, _HIDDEN, _COMPILE_ONLY) {
    REG(a);

    DPOP(a);

    a.as_de->flags ^= F_HIDDEN;
}


// ( -- )
PRIMITIVE ("HIDE", 0, _HIDE, _HIDDEN) {
    DPUSH((cell)(intptr_t) ' ');
    _WORD(NULL);
    _FIND(NULL);
    _HIDDEN(NULL);
}


// ( -- )
PRIMITIVE ("'", 0, _tick, _HIDE) {
    DPUSH((cell)(intptr_t) ' ');
    _WORD(NULL);
    _FIND(NULL);
    _DEtoCFA(NULL);
}


// ( -- )
PRIMITIVE ("BRANCH", 0, _BRANCH, _tick) {
    docolon_mode = DM_BRANCH;
}


// ( cond -- )
PRIMITIVE ("0BRANCH", 0, _0BRANCH, _BRANCH) {
    REG(a);

    DPOP(a);
    if (a.as_i == 0)  docolon_mode = DM_BRANCH;
    else              docolon_mode = DM_SKIP;
}


// ( -- )
PRIMITIVE ("LITSTRING", 0, _LITSTRING, _0BRANCH) {
    docolon_mode = DM_SLITERAL;
}


// ( addr len -- )
PRIMITIVE ("TELL", 0, _TELL, _LITSTRING) {
    REG(a);
    REG(b);

    DPOP(a);  // len
    DPOP(b);  // addr
    while (a.as_u--) {
        putchar(*((char *) b.as_ptr++));
    }
}


// ( -- status )
PRIMITIVE ("UGROW", 0, _UGROW, _TELL) {
    REG(a);

    a.as_i = mem_grow(var_UINCR->as_u);
    DPUSH(a);
}


// ( ncells -- status )
PRIMITIVE ("UGROWN", 0, _UGROWN, _UGROW) {
    REG(a);

    DPOP(a);
    a.as_i = mem_grow(a.as_u);
    DPUSH(a);
}

// ( ncells -- status )
PRIMITIVE ("USHRINK", 0, _USHRINK, _UGROWN) {
    REG(a);

    DPOP(a);
    a.as_i = mem_shrink(a.as_u);
    DPUSH(a);
}


// ( -- )
PRIMITIVE ("QUIT", 0, _QUIT, _USHRINK) {
    vm_quit();
}


// ( -- )
PRIMITIVE ("ABORT", 0, _ABORT, _QUIT) {
    fprintf(stderr, "ABORT called, rebooting\n");
    vm_abort();
}


// ( -- )
PRIMITIVE ("breakpoint", F_IMMED, _breakpoint, _ABORT) {
    REG(a);

    DPUSH((cell)(intptr_t) 1);
    DPOP(a);
}


// ( a -- a * sizeof(cell))
PRIMITIVE ("CELLS", 0, _CELLS, _breakpoint) {
    REG(a);

    DPOP(a);
    a.as_i *= sizeof(cell);
    DPUSH(a);
}


// (a -- a / sizeof(cell))
PRIMITIVE ("/CELLS", 0, _divCELLS, _CELLS) {
    REG(a);
    REG(b);

    DPOP(a);
    // signed/unsigned converts one to the other first
    b.as_i = sizeof(cell);
    DPUSH((cell)(intptr_t) (a.as_i / b.as_i));
}


// ( xt -- )
PRIMITIVE ("EXECUTE", 0, _EXECUTE, _divCELLS) {
    REG(xt);

    DPOP(xt);
    execute(xt.as_xt);
}


// ( "word" -- )
PRIMITIVE ("POSTPONE", F_IMMED | F_COMPONLY, _POSTPONE, _EXECUTE) {
    REG(a);
    CountedString *word;

    DPUSH((cell)(intptr_t) ' ');
    _WORD(NULL);
    DPEEK(a);
    word = a.as_cs;
    _FIND(NULL);
    DPOP(a);
    if (a.as_i) {
        DictEntry *de = a.as_de;
        if ((de->flags & F_IMMED) != 0) {
            // Immediate words are simply compiled
            DPUSH((cell) DE_to_CFA(de));
            _comma(NULL);
        }
        else {
            // For normal words, compile a compiler
            DPUSH((cell) DE_to_CFA(&_dict__LIT));
            _comma(NULL);
            DPUSH((cell) DE_to_CFA(de));
            _comma(NULL);
            DPUSH((cell) DE_to_CFA(&_dict__comma));
            _comma(NULL);
        }
    }
    else {
        // Couldn't find the word
        fprintf(stderr, "unrecognised word: %.*s\n", word->length, word->value);
        _QUIT(NULL);
    }
}


// ( n -- )
PRIMITIVE ("THROW", 0, _THROW, _POSTPONE) {
    REG(n);

    DPOP(n); 

    throw(n.as_i);
}


// ( xt -- status )
PRIMITIVE ("CATCH", 0, _CATCH, _THROW) {
    REG(xt);

    DPOP(xt);

    catch(xt.as_xt);
}


/***************************************************************************
    The LATEST variable denotes the top of the dictionary.  Its initial
    value points to its own dictionary entry (tricky).
    IMPORTANT:
    * Be sure to update its link pointer if you add more builtins before it!
    * This must be the LAST entry added to the dictionary!
 ***************************************************************************/
VARIABLE (LATEST, (intptr_t)&_dict_var_LATEST, 0, _CATCH);  // FIXME keep this updated!
