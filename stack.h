#ifndef _STACK_H
#define _STACK_H

#include <stdint.h>
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

static inline uintptr_t stack_count (const Stack *stack) {
    return (stack->index >= 0 ? stack->index + 1 : 0);
}

static inline void stack_pick (Stack *stack, unsigned int n) {
    // FIXME error checking
    stack->values[stack->index + 1] = stack->values[stack->index - n];
    ++ stack->index;
}

static inline void stack_roll (Stack *stack, unsigned int n) {
    // FIXME error checking
    register cell a = stack->values[stack->index - n];
    memmove(&stack->values[stack->index - n],       // dst
            &stack->values[1 + stack->index - n],   // src
            n * sizeof(cell));                      // len
    stack->values[stack->index] = a;
}

#endif
