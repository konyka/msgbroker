#ifndef MB_ALLOC_H_INCLUDED
#define MB_ALLOC_H_INCLUDED

#include <stddef.h>

void mb_alloc_init (void);
void mb_alloc_term (void);

void *mb_alloc (size_t size);
void *mb_realloc (void *ptr, size_t size);
void mb_free (void *ptr);

#define mb_alloc_assert(ptr) \
    do { if (!(ptr)) abort(); } while (0)

#endif
