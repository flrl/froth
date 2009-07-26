#ifndef _CELL_H
#define _CELL_H

#include <stdint.h>

struct _dict_entry;
struct _counted_string;

typedef void (*pvf)(void *);

typedef union _cell {
    intptr_t    as_i;
    uintptr_t   as_u;
    struct _dict_entry *as_de;
    pvf         as_pvf;
    pvf         *as_xt;
    union _cell *as_dfa;
    void        *as_ptr;   
    struct _counted_string *as_cs;
} cell;

#define CELL(X)  ((cell)(intptr_t) (X))

#endif
