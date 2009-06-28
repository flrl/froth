#ifndef _CELL_H
#define _CELL_H

#include <stdint.h>

struct _dict_entry;

typedef void (*pvf)();
typedef intptr_t cell;

typedef union _cell {
    intptr_t    as_i;
    uintptr_t   as_u;
    struct _dict_entry *as_de;
    pvf         as_pvf;
    pvf         *as_xt;
    union _cell *as_dfa;
} new_cell;
//} cell;

#endif
