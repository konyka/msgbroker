#ifndef MB_CHUNK_H_INCLUDED
#define MB_CHUNK_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

int mb_chunk_alloc (size_t size, void **result);
int mb_chunk_realloc (size_t size, void **chunk);
void mb_chunk_free (void *p);
void mb_chunk_addref (void *p, uint32_t n);
size_t mb_chunk_size (void *p);
void *mb_chunk_trim (void *p, size_t n);

#endif
