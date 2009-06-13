#ifndef _FORTH_H
#define _FORTH_H

#include <stdint.h>

#include "stack.h"

typedef void (*pvf)();

#define MAX_WORD_LEN (31)

typedef struct _dict_entry {
    struct _dict_entry *link;
    uint8_t     name_length; /* and flags, later */
    char        name[MAX_WORD_LEN];
    pvf         code;
    cell        param[];
} DictEntry;

/* Pre-declare this so the builtins can use it */
extern cell * const var_LATEST;


extern Stack    parameter_stack;
extern cell     W;




#endif
