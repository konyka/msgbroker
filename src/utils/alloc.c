#include "alloc.h"

#include <stdlib.h>
#include <string.h>

void mb_alloc_init (void)
{
}

void mb_alloc_term (void)
{
}

void *mb_alloc (size_t size)
{
    void *p = malloc (size);
    return p;
}

void *mb_realloc (void *ptr, size_t size)
{
    return realloc (ptr, size);
}

void mb_free (void *ptr)
{
    free (ptr);
}
