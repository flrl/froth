#ifndef _STACK_H
#define _STACK_H

#include <stdint.h>
#include <string.h>

#include "cell.h"
#include "exception.h"

#ifndef STACK_EMPTY
#define STACK_EMPTY (-1)
#endif

extern void throw();

typedef struct _stack {
    int32_t top;
    int underflow;
    int overflow;
    #define STACK_SIZE (8)
    cell values[STACK_SIZE];
} Stack;

static inline void stack_init (Stack *stack, int underflow, int overflow) {
    memset(stack, 0, sizeof (Stack));
    stack->top = STACK_EMPTY;
    stack->underflow = underflow;
    stack->overflow = overflow;
}

static inline void stack_push (Stack *stack, cell value) {
    if (stack->top >= STACK_SIZE - 1)  throw(stack->overflow);
    stack->values[++stack->top] = value;
}

static inline cell stack_pop (Stack *stack) {
    if (stack->top <= STACK_EMPTY)  throw(stack->underflow);
    return stack->values[stack->top--];
}

static inline cell stack_peek (const Stack *stack) {
    if (stack->top <= STACK_EMPTY)  throw(stack->underflow);
    return stack->values[stack->top];
}

static inline uintptr_t stack_count (const Stack *stack) {
    return (stack->top >= 0 ? stack->top + 1 : 0);
}

static inline void stack_pick (Stack *stack, unsigned int n) {
    if (stack->top >= STACK_SIZE - 1)  throw(stack->overflow);
    if (stack->top <= STACK_EMPTY + n)  throw(stack->underflow);
    stack->values[stack->top + 1] = stack->values[stack->top - n];
    ++ stack->top;
}

static inline void stack_roll (Stack *stack, unsigned int n) {
    if (stack->top >= STACK_SIZE - 1)  throw(stack->overflow);
    if (stack->top <= STACK_EMPTY + n)  throw(stack->underflow);
    register cell a = stack->values[stack->top - n];
    memmove(&stack->values[stack->top - n],     // dst
            &stack->values[1 + stack->top - n], // src
            n * sizeof(cell));                  // len
    stack->values[stack->top] = a;
}

#endif
