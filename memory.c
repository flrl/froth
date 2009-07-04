
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
    -> mem_grow
  * function to release space from the end of the region back to the system
    -> mem_shrink

  * private variable pointing to start of memory region 
    -> mem_start, and mem_get_start()
  * private variable containing length of memory region
    -> mem_ncells, and mem_get_ncells()

  * forth variable containing additional allocation size
    -> UINCR
  * forth variable containing reallocation threshold
    -> UTHRES
  * forth variable pointing to first free byte of memory region ("HERE")
    -> HERE

  * forth word to get the current size of user memory (in cells)
    -> USIZE
  * forth word to get the address of the start of user memory
    -> U0
  * forth word to force reallocation of additional space
    -> UGROW, UGROWN
  * forth word to release space from the end of the region back to the system
    -> USHRINK


*/

#include <stdio.h>  /* fprintf */
#include <stdlib.h> /* malloc, realloc */

#include "forth.h"
#include "memory.h"

/* Private state */
static cell     *mem_start = NULL;
static size_t   mem_ncells = 0;

/* These are copied from builtin.h, for easy reference while reading */
extern cell * const var_UINCR;
extern cell * const var_UTHRES;
extern cell * const var_HERE;


// You would normally call this with ncells = INIT_USIZE
void mem_init (size_t ncells) {
    if (mem_start)  free(mem_start);

    if (ncells == 0)  ncells = INIT_USIZE;

    mem_start = malloc(sizeof(cell) * ncells);
    // FIXME malloc error checking

    mem_ncells = ncells;
    var_HERE->as_dfa = mem_start;

    var_UINCR->as_u = INIT_UINCR;
    var_UTHRES->as_u = INIT_UTHRES;
}


// Should only need to call this right before exit.  Use mem_init again 
// to reinitialise while running
void mem_destroy () {
    if (mem_start)  free(mem_start);

    mem_start = NULL;
    mem_ncells = 0;

    var_HERE->as_dfa = mem_start;

    var_UINCR->as_u = 0;
    var_UTHRES->as_u = 0;
}


int mem_shouldgrow () {
    size_t used_cells = (var_HERE->as_dfa - mem_start) / sizeof(cell);
    if (mem_ncells - used_cells < var_UTHRES->as_u) {
        return 1;
    }
    else {
        return 0;
    }
}


int mem_canshrink () {
    size_t used_cells = (var_HERE->as_dfa - mem_start) / sizeof(cell);
    if (used_cells < mem_ncells - var_UTHRES->as_u) {
        return 1;
    }
    else {
        return 0;
    }
}


// You would normally call this with ncells = *var_UINCR
int mem_grow (size_t ncells) {
    cell *new_mem_start;
    size_t new_mem_ncells = mem_ncells + ncells;

    fprintf(stderr, "Attempting to grow user memory by %zu cells (%zu bytes)... ",
        ncells, sizeof(cell) * ncells);
    new_mem_start = realloc(mem_start, sizeof(cell) * new_mem_ncells);
    if (new_mem_start == NULL) {
        // realloc failed
        // original pointer is STILL GOOD, so don't change anything 
        // (but we might run out of memory soon)
        // FIXME maybe print a warning or something
        fprintf(stderr, "failed!\n");
        return -1;
    }
    else if (new_mem_start != mem_start) {
        // realloc succeeded, but our memory region has moved
        size_t here_offset = var_HERE->as_dfa - mem_start;
        mem_start = new_mem_start;
        mem_ncells = new_mem_ncells;
        var_HERE->as_dfa = mem_start + here_offset;
        fprintf(stderr, "succeeded (with relocation)\n");
    }
    else {
        // realloc succeeded, memory region is in the same place but is just longer
        mem_ncells = new_mem_ncells;
        fprintf(stderr, "succeeded (in place)\n");
    }
    return 0;
}


int mem_shrink (size_t ncells) {
    cell *new_mem_start;
    size_t new_mem_ncells = mem_ncells - ncells;

    // Refuse to release cells if doing so would invalidate HERE
    if ((mem_start + sizeof(cell) * new_mem_ncells) <= *(cell**)var_HERE) {
        fprintf(stderr, "Rejected attempt to shrink user memory while still in use\n");    
        return -1;
    }

    fprintf(stderr, "Attempting to shrink user memory by %zu cells (%zu bytes)... ",
        ncells, sizeof(cell) * ncells);
    new_mem_start = realloc(mem_start, sizeof(cell) * new_mem_ncells);
    if (new_mem_start == NULL) {
        // Shrinking reallocation failed, WTF
        // Original Pointer is still good, so don't change anything
        // FIXME maybe print a warning or something
        fprintf(stderr, "failed! (wtf)\n");
    }
    else if (new_mem_start != mem_start) {
        // I cannot think of a scenario where it would need to relocate to shrink?
        size_t here_offset = var_HERE->as_dfa - mem_start;
        mem_start = new_mem_start;
        mem_ncells = new_mem_ncells;
        * (cell**) var_HERE = mem_start + here_offset;
        fprintf(stderr, "succeeded (with relocation -- wtf)\n");
    }
    else {
        // realloc succeeded, memory region is in the same place but is shorter
        mem_ncells = new_mem_ncells;
        fprintf(stderr, "succeeded (in place)\n");
    }
    return 0;
}


// Returns the address where the user memory starts.  
// This is safe -- it's *not* returning the address of our private pointer 
// to it (so we're not exposed to external modification), but merely the 
// value of our private pointer.  Enterprising forth programmers may use 
// this value to fuck up their own memory if they like :P
cell *mem_get_start () {
    return mem_start;
}


// Returns the size of the user memory region, in cells.
size_t mem_get_ncells () {
    return mem_ncells;
}
