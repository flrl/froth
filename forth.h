#ifndef _FORTH_H
#define _FORTH_H

#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>

#include "cell.h"
#include "exception.h"
#include "stack.h"
#include "builtin.h"
#include "memory.h"




#define MAX_WORD_LEN    (31)
#define MAX_ERROR_LEN   (1024)
#define SENTINEL        (0xCAFEBABE)

/* Initial memory sizes, in cells */
#define INIT_USIZE      (4096) 
#define INIT_UINCR      (1024)
#define INIT_UTHRES     (1024)

typedef struct _dict_header {
    struct _dict_entry *link;
    uint8_t     flags;
    char        name[MAX_WORD_LEN];
    uint32_t    sentinel;
    pvf         code;
} DictHeader;

typedef struct _dict_entry {
    struct _dict_entry *link;
    uint8_t     flags;
    char        name[MAX_WORD_LEN];
    uint32_t    sentinel;
    pvf         code;
    cell        param[];
} DictEntry;

//
typedef struct _dict_entry_new {
    struct _dict_entry *link;
      uint8_t     flags1;
      char        name[MAX_WORD_LEN];
    uint32_t    flags2;
    pvf         code;
    cell        param[];
} NewDictEntry;
//

#define DE_to_CFA(DE)   (pvf*)(((void*)(DE)) + offsetof(struct _dict_entry, code))
#define DE_to_DFA(DE)   (cell*)(((void*)(DE)) + offsetof(struct _dict_entry, param))
#define DE_to_SFA(DE)   (uint32_t*)(((void*)(DE)) + offsetof(struct _dict_entry, sentinel))
#define DE_to_NAME(DE)  (CountedString*)(((void*)(DE)) + offsetof(struct _dict_entry, flags))

#define CFA_to_DE(CFA)  (DictEntry*)(((void*)(CFA)) - offsetof(struct _dict_entry, code))
#define DFA_to_DE(DFA)  (DictEntry*)(((void*)(DFA)) - offsetof(struct _dict_entry, param))

#define CFA_to_DFA(CFA) DE_to_DFA(CFA_to_DE(CFA))
#define CFA_to_SFA(CFA) DE_to_SFA(CFA_to_DE(CFA))

#define DFA_to_CFA(DFA) DE_to_CFA(DFA_to_DE(DFA))

typedef struct _dict_debug {
    DictHeader  header;
    cell        param[40];
} DictDebug;

typedef struct _counted_string {
#define MAX_COUNTED_STRING_LENGTH (255)
    uint8_t     length;
    char        value[MAX_COUNTED_STRING_LENGTH];
} CountedString;

enum {
    F_IMMED = 0x80,
    F_COMPONLY = 0x40,
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

typedef enum {
    E_OK = 0,
    E_PARSE = 1,
} Error;

extern Stack    data_stack;
extern Stack    return_stack;
extern Stack    control_stack;

extern InterpreterState interpreter_state;
extern DocolonMode docolon_mode;
extern Error error_state;
extern char  error_message[];

extern jmp_buf cold_boot; //?
extern jmp_buf warm_boot; //?
extern ExceptionFrame exception_frame;

extern void do_interpret (void*);

#endif
