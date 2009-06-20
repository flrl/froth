#ifndef _STACK_H
#define _STACK_H

#include <string.h>

#include "cell.h"

typedef struct _stack {
    int32_t  index;
    cell values[256];
} Stack;

static inline void stack_init (Stack *stack) {
    memset(stack, 0, sizeof (Stack));
    stack->index = -1;
}

static inline void stack_push (Stack *stack, cell value) {
    stack->values[++stack->index] = value;
}

static inline cell stack_pop (Stack *stack) {
    return stack->values[stack->index--];
}

static inline cell stack_top (const Stack *stack) {
    return stack->values[stack->index];
}

#endif
