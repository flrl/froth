#ifndef _EXCEPTION_H
#define _EXCEPTION_H

#include <setjmp.h>
#include <stdint.h>

typedef struct _exception_frame {
    jmp_buf target;
    int16_t ds_top;
    int16_t rs_top;
    int16_t cs_top;
} ExceptionFrame;

void exception_init();

// increments stack top and returns pointer to the new top for caller to initialise
// returns NULL if the stack would overflow
ExceptionFrame * exception_next_frame();

// returns pointer to current top
// returns NULL if the stack is empty
ExceptionFrame * exception_current_frame();

// decrements stack top and returns pointer to the old top for caller to longjmp to
// returns NULL if the stack is empty
ExceptionFrame * exception_pop_frame();

// decrements stack top
void exception_drop_frame();



enum {
    EXC_COND  = -58,
    EXC_CHARIO,
    EXC_QUIT,
    EXC_FPERR,
    EXC_FPUNDER,
    EXC_EXOVER,
    EXC_CTRLOVER,
    EXC_COMPCHANGE,
    EXC_SEARCH_UNDER,
    EXC_SEARCH_OVER,
    EXC_POSTPONE,
    EXC_COMPDEL,
    EXC_FPINV,
    EXC_FS_UNDER,
    EXC_FS_OVER,
    EXC_FPRANGE,
    EXC_FPDIVZERO,
    EXC_FPPRECIS,
    EXC_FPBASE,
    EXC_EOF,
    EXC_NOFILE,
    EXC_FILEIO,
    EXC_INVFILEPOS,
    EXC_INVBLOCKNUM,
    EXC_BLOCKWRITE,
    EXC_BLOCKREAD,
    EXC_INVNAME,
    EXC_BODY,
    EXC_OBS,
    EXC_NESTCOMP,
    EXC_INTERRUPT,
    EXC_RECURSE,
    EXC_LOOP,
    EXC_RS_IMBAL,
    EXC_INV_NUM,
    EXC_ALIGN,
    EXC_CTRL,
    EXC_UNSUPPORTED,
    EXC_READONLY,
    EXC_NAMELEN,
    EXC_STR_OVER,
    EXC_PIC_OVER,
    EXC_EMPTY_NAME,
    EXC_FORGET,
    EXC_COMPONLY,
    EXC_UNDEF,
    EXC_ARG,
    EXC_RANGE,
    EXC_DIVZERO,
    EXC_INV_ADDR,
    EXC_DICT_OVER,
    EXC_DO_NEST,
    EXC_RS_UNDER,
    EXC_RS_OVER,
    EXC_DS_UNDER,
    EXC_DS_OVER,
    EXC_ABORTQ,
    EXC_ABORT,
    EXC_OK = 0,
};



#endif
