#ifndef _MEMORY_H
#define _MEMORY_H


void mem_init ();
void mem_destroy ();
int  mem_shouldgrow ();
int  mem_grow (size_t ncells);
int  mem_shrink (size_t ncells);
cell *mem_get_start ();
size_t mem_get_ncells ();


#endif /* _MEMORY_H */
