#ifndef _FORTH_H
#define _FORTH_H

#include <stdint.h>

#include "stack.h"
#include "builtin.h"
#include "memory.h"

typedef void (*pvf)();

#define MAX_WORD_LEN    (31)

/* Initial memory sizes, in cells */
#define INIT_USIZE      (4096) 
#define INIT_UINCR      (1024)
#define INIT_UTHRES     (1024)

typedef struct _dict_entry {
    struct _dict_entry *link;
    uint8_t     name_length; /* and flags, later */
    char        name[MAX_WORD_LEN];
    pvf         code;
    cell        param[];
} DictEntry;

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

extern Stack    parameter_stack;
extern cell     W;

extern InterpreterState interpreter_state;
extern DocolonMode docolon_mode;


#endif
