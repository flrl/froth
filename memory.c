
/*

  Needs:

  * initial size for memory region
    -> INIT_USIZE (macro in forth.h)
  * default additional size to allocate when the current region is "nearly full"
    -> INIT_UINCR (macro in forth.h)
  * mininum unused space threshold after which the region is deemed "nearly full" and additional
    space is allocated
    -> INIT_UTHRES (macro in forth.h)
  * function to perform an initial allocation
    -> mem_init
  * function to allocate additional space
    -> mem_allocate
  * function to release space from the end of the region back to the system
    -> mem_release

  * private variable pointing to start of memory region 
    -> mem_start
  * private variable containing length of memory region
    -> mem_size
  * forth variable mirroring mem_size (forth code MUST NOT change this, but may read it)
    -> USIZE
  * forth variable containing additional allocation size
    -> UINCR
  * forth variable containing reallocation threshold
    -> UTHRES
  * forth variable mirroring mem_start (forth code MUST NOT change the pointer, but MAY read it)
    -> U0
  * forth variable pointing to first free byte of memory region ("HERE")
    -> HERE

  * forth word to force reallocation of additional space
  * forth word to release space from the end of the region back to the system

*/

#include <stdlib.h>

#include "forth.h"
#include "memory.h"

/* Private state */
static cell     *mem_start = NULL;
static size_t   mem_cells = 0;

/* These are copied from builtin.h */
extern cell * const var_USIZE;
extern cell * const var_UINCR;
extern cell * const var_UTHRES;
extern cell * const var_U0;
extern cell * const var_HERE;


// You would normally call this with ncells = INIT_USIZE
void mem_init (size_t ncells) {
    if (mem_start)  free(mem_start);

    if (ncells == 0)  ncells = INIT_USIZE;

    mem_start = malloc(sizeof(cell) * ncells);
    // FIXME malloc error checking

    *var_USIZE = mem_cells = ncells;
    *var_U0 = *var_HERE = (cell) mem_start;

    *var_UINCR = INIT_UINCR;
    *var_UTHRES = INIT_UTHRES;
}

// Should only need to call this right before exit.  Use mem_init again 
// to reinitialise while running
void mem_destroy () {
    if (mem_start)  free(mem_start);

    mem_start = NULL;
    mem_cells = 0;

    *var_USIZE = mem_cells;
    *var_U0 = *var_HERE = (cell) mem_start;

    *var_UINCR = 0;
    *var_UTHRES = 0;
}

// You would normally call this with ncells = *var_UINCR
void mem_allocate (size_t ncells) {
    cell *new_mem_start;
    size_t new_mem_cells = mem_cells + ncells;

    new_mem_start = realloc(mem_start, sizeof(cell) * new_mem_cells);
    if (new_mem_start == NULL) {
        // realloc failed, original pointer is STILL GOOD
        // so don't change anything (but we might run out of memory soon)
        // FIXME maybe print a warning or something
    }
    else if (new_mem_start != mem_start) {
        // realloc succeeded, but our memory region has moved
        size_t here_offset = (cell *) *var_HERE - mem_start;
        * (cell**) var_U0 = mem_start = new_mem_start;
        *var_USIZE = mem_cells = new_mem_cells;
        * (cell**) var_HERE = mem_start + here_offset;
    }
    else {
        // realloc succeeded, memory region is in the same place but is just longer
        *var_USIZE = mem_cells = new_mem_cells;
    }
}


void mem_release (size_t ncells) {
    cell *new_mem_start;
    size_t new_mem_cells = mem_cells - ncells;

    // Refuse to release cells if doing so would invalidate HERE
    if ((mem_start + sizeof(cell) * new_mem_cells) <= *(cell**)var_HERE)  return;

    new_mem_start = realloc(mem_start, sizeof(cell) * new_mem_cells);
    if (new_mem_start == NULL) {
        // Shrinking reallocation failed, WTF
        // Original Pointer is still good, so don't change anything
        // FIXME maybe print a warning or something
    }
    else if (new_mem_start != mem_start) {
        // I cannot think of a scenario where it would need to relocate to shrink?
        size_t here_offset = (cell *) *var_HERE - mem_start;
        * (cell**) var_U0 = mem_start = new_mem_start;
        *var_USIZE = mem_cells = new_mem_cells;
        * (cell**) var_HERE = mem_start + here_offset;
    }
    else {
        // realloc succeeded, memory region is in the same place but is shorter
        *var_USIZE = mem_cells = new_mem_cells;
    }
}



