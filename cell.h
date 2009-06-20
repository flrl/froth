#ifndef _CELL_H
#define _CELL_H

#include <stdint.h>

typedef void (*pvf)();
typedef intptr_t cell;

typedef union _cell {
    intptr_t    i;
    uintptr_t   u;
    union _cell *pfa;
    pvf         *cfa;
} new_cell;

#endif
