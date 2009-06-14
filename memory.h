#ifndef _MEMORY_H
#define _MEMORY_H


void mem_init ();
void mem_destroy ();
void mem_allocate (size_t ncells);
void mem_release (size_t ncells);


#endif /* _MEMORY_H */
