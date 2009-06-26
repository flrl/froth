#ifndef _FORTH_H
#define _FORTH_H

#include <stdint.h>
#include <setjmp.h>

#include "cell.h"
#include "stack.h"
#include "builtin.h"
#include "memory.h"




#define MAX_WORD_LEN    (31)
#define MAX_ERROR_LEN   (1024)
#define SENTINEL        (0xCAFEBABE)

/* Initial memory sizes, in cells */
#define INIT_USIZE      (4096) 
#define INIT_UINCR      (1024)
#define INIT_UTHRES     (1024)

typedef struct _dict_header {
    struct _dict_entry *link;
    uint8_t     name_length;
    char        name[MAX_WORD_LEN];
    uint32_t    sentinel;
} DictHeader;

typedef struct _dict_entry {
    struct _dict_entry *link;
    uint8_t     name_length;
    char        name[MAX_WORD_LEN];
    uint32_t    sentinel;
    pvf         code;
    cell        param[];
} DictEntry;

typedef struct _dict_debug {
    DictHeader  header;
    pvf         code;
    cell        param[40];
} DictDebug;

enum {
    F_IMMED = 0x80,
    F_HIDDEN = 0x20,
    F_LENMASK = 0x1F,
};

typedef enum {
    S_INTERPRET = 0,
    S_COMPILE = 1,
} InterpreterState;

typedef enum {
    DM_SKIP = -1,
    DM_NORMAL = 0,
    DM_BRANCH = 1,
    DM_LITERAL = 2,
    DM_SLITERAL = 3,
} DocolonMode;

typedef enum {
    E_OK = 0,
    E_PARSE = 1,
} Error;

extern Stack    parameter_stack;
extern Stack    return_stack;
extern Stack    control_stack;

extern InterpreterState interpreter_state;
extern DocolonMode docolon_mode;
extern Error error_state;
extern char  error_message[];

extern jmp_buf cold_boot; //?
extern jmp_buf warm_boot; //?

extern void do_interpret (void*);

#endif
