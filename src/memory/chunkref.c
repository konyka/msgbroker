#include "chunkref.h"
#include "chunk.h"
#include "../utils/alloc.h"
#include "../utils/fast.h"

#include <string.h>

void mb_chunkref_init (struct mb_chunkref *self, size_t size)
{
    self->size = size;
    if (size <= MB_CHUNKREF_MAX) {
        memset (self->u.data, 0, size);
    } else {
        void *chunk = NULL;
        mb_chunk_alloc (size, &chunk);
        self->u.chunk = chunk;
    }
}

void mb_chunkref_init_chunk (struct mb_chunkref *self, void *chunk,
    size_t offset, size_t size)
{
    self->size = size;
    if (size <= MB_CHUNKREF_MAX) {
        memcpy (self->u.data, (char *) chunk + offset, size);
    } else {
        self->u.chunk = (char *) chunk + offset;
        mb_chunk_addref (self->u.chunk, 1);
    }
}

void mb_chunkref_term (struct mb_chunkref *self)
{
    if (self->size > MB_CHUNKREF_MAX && self->u.chunk)
        mb_chunk_free (self->u.chunk);
    self->size = 0;
    self->u.chunk = NULL;
}

void mb_chunkref_mv (struct mb_chunkref *dst, struct mb_chunkref *src)
{
    if (src->size == 0) {
        dst->size = 0;
        dst->u.chunk = NULL;
        src->size = 0;
        src->u.chunk = NULL;
        return;
    }
    memcpy (dst, src, sizeof (struct mb_chunkref));
    src->size = 0;
    src->u.chunk = NULL;
}

void mb_chunkref_cp (struct mb_chunkref *dst, struct mb_chunkref *src)
{
    dst->size = src->size;
    if (src->size <= MB_CHUNKREF_MAX) {
        memcpy (dst->u.data, src->u.data, src->size);
    } else {
        mb_chunk_addref (src->u.chunk, 1);
        dst->u.chunk = src->u.chunk;
    }
}

void *mb_chunkref_data (struct mb_chunkref *self)
{
    if (self->size <= MB_CHUNKREF_MAX)
        return self->u.data;
    return self->u.chunk;
}

size_t mb_chunkref_size (struct mb_chunkref *self)
{
    return self->size;
}

void mb_chunkref_set (struct mb_chunkref *self, const void *data,
    size_t size)
{
    if (self->size > MB_CHUNKREF_MAX && self->u.chunk)
        mb_chunk_free (self->u.chunk);
    self->size = size;
    if (size <= MB_CHUNKREF_MAX) {
        memcpy (self->u.data, data, size);
    } else {
        void *chunk = NULL;
        mb_chunk_alloc (size, &chunk);
        memcpy (chunk, data, size);
        self->u.chunk = chunk;
    }
}

void mb_chunkref_resize (struct mb_chunkref *self, size_t size)
{
    if (self->size <= MB_CHUNKREF_MAX && size <= MB_CHUNKREF_MAX) {
        self->size = size;
        return;
    }
    if (self->size > MB_CHUNKREF_MAX && size > MB_CHUNKREF_MAX) {
        mb_chunk_realloc (size, &self->u.chunk);
        self->size = size;
        return;
    }
    struct mb_chunkref tmp;
    mb_chunkref_init (&tmp, size);
    size_t tocopy = self->size < size ? self->size : size;
    memcpy (mb_chunkref_data (&tmp), mb_chunkref_data (self), tocopy);
    mb_chunkref_term (self);
    mb_chunkref_mv (self, &tmp);
}
