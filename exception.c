#include <string.h>

#include "exception.h"

#ifndef STACK_EMPTY
#define STACK_EMPTY (-1)
#endif

static struct {
    int32_t top;
#define EXCEPTION_STACK_SIZE (32)
    ExceptionFrame values[EXCEPTION_STACK_SIZE];
} exception_stack;

void exception_init() {
    memset(&exception_stack, 0, sizeof(exception_stack));
    exception_stack.top = STACK_EMPTY;
}

// increments stack top and returns pointer to the new top for caller to initialise
// returns NULL if the exception stack would overflow
ExceptionFrame * exception_next_frame() {
    if (exception_stack.top >= EXCEPTION_STACK_SIZE - 1)  return NULL;

    return &exception_stack.values[++exception_stack.top];
}

// returns pointer to current top
// returns NULL if the exception stack is empty
ExceptionFrame * exception_current_frame() {
    if (exception_stack.top <= STACK_EMPTY)  return NULL;

    return &exception_stack.values[exception_stack.top];
}

// decremends stack top and returns pointer to the old top for caller to longjmp to
// returns NULL if the exception stack is empty
ExceptionFrame * exception_pop_frame() {
    if (exception_stack.top <= STACK_EMPTY)  return NULL;

    return &exception_stack.values[exception_stack.top--];
}

void exception_drop_frame() {
    if (exception_stack.top > STACK_EMPTY)  exception_stack.top--;
}
