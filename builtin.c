#include <stdio.h>
#include <stdlib.h>

#include "forth.h"

// Function signature for a primitive
#define DECLARE_PRIMITIVE(P)    void P (void *pfa)

// Define a primitive and add it to the dictionary
#define PRIMITIVE(NAME, FLAGS, CNAME, LINK)                                 \
    DECLARE_PRIMITIVE(CNAME);                                               \
    DictEntry _dict_##CNAME =                                               \
        { &_dict_##LINK, ((FLAGS) | (sizeof(NAME) - 1)), NAME, CNAME, };    \
    DECLARE_PRIMITIVE(CNAME)

// Define a variable and add it to the dictionary; also create a pointer for direct access
#define VARIABLE(NAME, INITIAL, FLAGS, LINK)                                                \
    DictEntry _dict_var_##NAME =                                                            \
        { &_dict_##LINK, ((FLAGS) | (sizeof(#NAME) - 1)), #NAME, do_variable, {INITIAL} };  \
    cell * const var_##NAME = &_dict_var_##NAME.param[0]

// Define a constant and add it to the dictionary; also create a pointer for direct access
#define CONSTANT(NAME, VALUE, FLAGS, LINK)                                                  \
    DictEntry _dict_const_##NAME =                                                          \
        { &_dict_##LINK, ((FLAGS) | (sizeof(#NAME) - 1)), #NAME, do_constant, { VALUE } };  \
    const cell * const const_##NAME = &_dict_const_##NAME.param[0]

// Shorthand macros to make repetitive code more writeable, but possibly less readable
#define REG(X)        register cell X
#define PTOP(X)       X = stack_top(&parameter_stack)
#define PPOP(X)       X = stack_pop(&parameter_stack)
#define PPUSH(X)      stack_push(&parameter_stack, (X))





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

void do_colon2(void *cfa) {
    // This version expects its argument to point to the CFA rather than the PFA
    for (pvf **code = (pvf **) cfa + 1; code && *code; code++) {
        (***code)(*code);
    }
}



/*
    pfa     = parameter field address, contains address of parameter field
    *pfa    = parameter field, contains address of code field for next word
    **pfa   = code field, contains address of function to process the word
    ***pfa  = interpreter function for the word 
 */
void do_colon(void *pfa) {

    // deref pfa to get the cfa of the word to execute
    // increment pfa and do it again
    // stop when pfa derefs to null

//    pvf **param = (pvf **) pfa; // W?
//    for (int i=0; param[i] != NULL; i++) {
//        W = param[i] + 1;
//        (*param[i]) ( param[i] + 1 );
//    }
    *var_ESCLIT = 0;
    pvf **param;
    for (param = (pvf **) pfa; param && *param; param++) {
        pvf *code = *param;
        if (*var_ESCLIT) {
            PPUSH((cell) code);
            *var_ESCLIT = 0;
        }
        else {
            (**code) (code + 1);
        }
    }


//    pvf **param = (pvf **) pfa; // W?
//    while (param && *param) {  // param = 168
//        pvf *code = *param;    // code = 292
//        void *arg = code + 1;  // arg = 296
//        W = arg;

//        (**code) ( arg );

//        param++;
//    }
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
    NULL,   // code
};


/***************************************************************************
  Builtin variables -- keep these together
  Exception: LATEST should be declared very last (see end of file)
    VARIABLE(NAME, INITIAL, FLAGS, LINK)
***************************************************************************/
VARIABLE (STATE,    S_INTERPRET,    0,          __ROOT);        // default to interpret 
VARIABLE (BASE,     0,              0,          var_STATE);     // default to smart base
VARIABLE (UINCR,    INIT_UINCR,     0,          var_BASE);      //
VARIABLE (UTHRES,   INIT_UTHRES,    0,          var_UINCR);     //
VARIABLE (HERE,     0,              0,          var_UTHRES);    // default to NULL
VARIABLE (ESCLIT,   0,              F_HIDDEN,   var_HERE);      // default to false


/***************************************************************************
  Builtin constants -- keep these together
    CONSTANT(NAME, VALUE, FLAGS, LINK)
 ***************************************************************************/
CONSTANT (VERSION,      0,                      0,  var_ESCLIT);
CONSTANT (DOCOL,        (cell) &do_colon,       0,  const_VERSION);
CONSTANT (DOVAR,        (cell) &do_variable,    0,  const_DOCOL);
CONSTANT (DOCON,        (cell) &do_constant,    0,  const_DOVAR);
CONSTANT (F_IMMED,      F_IMMED,                0,  const_DOCON);
CONSTANT (F_HIDDEN,     F_HIDDEN,               0,  const_F_IMMED);
CONSTANT (F_LENMASK,    F_LENMASK,              0,  const_F_HIDDEN);
CONSTANT (S_INTERPRET,  S_INTERPRET,            0,  const_F_LENMASK);
CONSTANT (S_COMPILE,    S_COMPILE,              0,  const_S_INTERPRET);
CONSTANT (SHEEP,        0xDEADBEEF,             0,  const_S_COMPILE);


/***************************************************************************
  Builtin code words -- keep these together
    PRIMITIVE(NAME, FLAGS, CNAME, LINK)
 ***************************************************************************/

// ( a -- )
PRIMITIVE ("DROP", 0, _DROP, const_S_COMPILE) {
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


// ( d c b a -- b a d c ) 
PRIMITIVE ("2SWAP", 0, _2SWAP, _2DUP) {
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
    PPOP(a);
    PPUSH(a == b);
}


// ( b a -- a != b )
PRIMITIVE ("<>", 0, _notequals, _equals) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(a);
    PPUSH(a != b);
}


// ( b a -- b < a )
PRIMITIVE ("<", 0, _lt, _notequals) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(a);
    PPUSH(b < a);
}


// ( b a -- b > a )
PRIMITIVE (">", 0, _gt, _lt) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(a);
    PPUSH(b > a);
}


// ( b a -- b <= a )
PRIMITIVE ("<=", 0, _lte, _gt) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(a);
    PPUSH(b <= a);
}


// ( b a -- b >= a )
PRIMITIVE (">=", 0, _gte, _lte) {
    REG(a);
    REG(b);

    PPOP(a);
    PPOP(a);
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

// ( -- addr )
PRIMITIVE ("U0", 0, _U0, _INVERT) {
    REG(a);

    a = (cell) mem_get_start();
    PPUSH(a);
}


// ( -- ncells )
PRIMITIVE ("USIZE", 0, _USIZE, _U0) {
    REG(a);

    a = mem_get_ncells();
    PPUSH(a);
}


// ( value addr -- )
PRIMITIVE ("!", 0, _store, _USIZE) {
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


// ( FIXME )
PRIMITIVE ("C@C!", 0, _ccopy, _fetchbyte) {
    // FIXME wtf
    // maybe this is supposed to take two addresses and copy a byte from one to the other?
    // but then wtf is "increment destination", "increment source" about?
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


// ( -- char )
PRIMITIVE ("KEY", 0, _KEY, _cmove) {
    REG(a);

    /* stdio is line buffered when attached to terminals :) */
    a = fgetc(stdin);
    PPUSH(a);
}


// ( char -- )
PRIMITIVE ("EMIT", 0, _EMIT, _KEY) {
    REG(a);

    PPOP(a);
    fputc(a, stdout);
}


// ( -- addr len )
PRIMITIVE ("WORD", 0, _WORD, _EMIT) {
    static char buf[32];
    REG(a);
    REG(b);

    /* First skip leading whitespace or skip-to-eol's */
    b = 0; // Flag for skip-to-eol
    do {
        _KEY(NULL);
        PPOP(a);
        if (a == '\\')  b = 1;
        else if (b && a == '\n')  b = 0;
    } while ( b || a == ' ' || a == '\t' || a == '\n' );

    /* Then start storing chars in the buffer */
    b = 0; // Position within array
    do {
        _KEY(NULL);
        PPOP(a);
        buf[b++] = a;
    } while ( b < MAX_WORD_LEN && a != ' ' && a != '\t' && a != '\n' );

    if (b < MAX_WORD_LEN) {
        // If the word we read was of a suitable length, return addr and len
        PPUSH((cell)(&buf));
        PPUSH(b);
    }
    else {
        // If we got to the end without finding whitespace, return some sort of error
        // FIXME really?
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


// ( addr len -- addr )
PRIMITIVE ("FIND", 0, _FIND, _NUMBER) {
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
            }
        }
        de = de->link;
    }

    PPUSH((cell) result);
}


// ( addr -- cfa )
PRIMITIVE (">CFA", 0, _toCFA, _FIND) {
/*
struct _dict_entry  *link;
uint8_t             name_length; 
char                name[MAX_WORD_LEN];
pvf                 code;
*/
    REG(a);

    PPOP(a);
    PPUSH(a + sizeof(DictEntry*) + sizeof(uint8_t) + MAX_WORD_LEN);
}


// ( addr -- dfa )
PRIMITIVE (">DFA", 0, _toDFA, _toCFA) {
    REG(a);

    PPOP(a);
    PPUSH(a + sizeof(DictEntry*) + sizeof(uint8_t) + MAX_WORD_LEN + sizeof(pvf));
}

// ( -- )
PRIMITIVE ("LIT", 0, _LIT, _toDFA) {
    *var_ESCLIT = 1;
}


// ( FIXME )
PRIMITIVE ("CREATE", 0, _CREATE, _LIT) {
    // FIXME
}


// FIXME other stuff



// ( -- status )
PRIMITIVE ("UGROW", 0, _UGROW, _CREATE) {
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

/***************************************************************************
    The LATEST variable denotes the top of the dictionary.  Its initial
    value points to its own dictionary entry (tricky).
    IMPORTANT:
    * Be sure to update its link pointer if you add more builtins before it!
    * This must be the LAST entry added to the dictionary!
 ***************************************************************************/
VARIABLE (LATEST, (cell) &_dict_var_LATEST, 0, _USHRINK);  // FIXME keep this updated!
