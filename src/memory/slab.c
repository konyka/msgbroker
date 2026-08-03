#include "slab.h"
#include "../utils/alloc.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MB_SLAB_MAGIC 0x534C4142u  /* "SLAB" */
#define MB_SLAB_HDR_SIZE (sizeof (uint32_t) * 2)

void mb_slab_init (struct mb_slab *self, size_t obj_size, size_t capacity)
{
    size_t i;
    void **freelist;
    size_t alloc_size;

    self->obj_size = obj_size;
    self->count = 0;
    self->capacity = 0;
    self->freelist = NULL;

    if (capacity == 0 || obj_size == 0)
        return;
    if (capacity > SIZE_MAX / sizeof (void *))
        return;

    /*  Each object carries an 8-byte header (magic + obj_size) before
     *  the user-visible pointer so that a wrong-slab free can be
     *  detected. Allocate obj_size + MB_SLAB_HDR_SIZE bytes per object. */
    if (obj_size > SIZE_MAX - MB_SLAB_HDR_SIZE)
        return;
    alloc_size = obj_size + MB_SLAB_HDR_SIZE;

    freelist = (void **) mb_alloc (capacity * sizeof (void *));
    if (!freelist)
        return;

    for (i = 0; i < capacity; i++) {
        void *raw = mb_alloc (alloc_size);
        if (!raw) {
            if (i == 0) {
                mb_free (freelist);
                return;
            }
            self->freelist = freelist;
            self->capacity = i;
            return;
        }
        memset (raw, 0, alloc_size);
        freelist[i] = raw;
    }

    self->freelist = freelist;
    self->capacity = capacity;
}

void mb_slab_term (struct mb_slab *self)
{
    size_t i;

    if (!self->freelist) {
        self->capacity = 0;
        self->count = 0;
        return;
    }

    for (i = 0; i < self->capacity; i++) {
        if (self->freelist[i])
            mb_free (self->freelist[i]);
    }
    mb_free (self->freelist);
    self->freelist = NULL;
    self->capacity = 0;
    self->count = 0;
}

#define MB_SLAB_MAGIC 0x534C4142u  /* "SLAB" */
#define MB_SLAB_HDR_SIZE (sizeof (uint32_t) * 2)

void *mb_slab_alloc (struct mb_slab *self)
{
    void *raw;
    void *user;
    uint32_t *hdr;

    if (!self->freelist || self->capacity == 0)
        return NULL;
    /*  freelist is a fixed-size array of raw pointers; count is the
     *  next index to allocate. The free path backtracks count to the
     *  freed slot's index so a subsequent alloc reuses that slot. */
    if (self->count >= self->capacity)
        return NULL;
    raw = self->freelist[self->count];
    if (!raw)
        return NULL;
    self->count++;
    user = (char *) raw + MB_SLAB_HDR_SIZE;
    hdr = (uint32_t *) raw;
    hdr[0] = MB_SLAB_MAGIC;
    hdr[1] = (uint32_t) self->obj_size;
    return user;
}

void mb_slab_free (struct mb_slab *self, void *obj)
{
    void *raw;
    uint32_t *hdr;
    size_t i;

    if (!obj || !self->freelist)
        return;
    /*  Walk the freelist to confirm raw belongs to one of the live
     *  raw allocations before dereferencing the header. This avoids
     *  reading 8 bytes before a random foreign pointer. */
    raw = (char *) obj - MB_SLAB_HDR_SIZE;
    for (i = 0; i < self->capacity; ++i) {
        if (self->freelist[i] == raw) {
            hdr = (uint32_t *) raw;
            if (hdr[0] != MB_SLAB_MAGIC ||
                hdr[1] != (uint32_t) self->obj_size)
                return;
            hdr[0] = 0;
            hdr[1] = 0;
            /*  The freelist still owns raw at index i. count moves
             *  back to i so the next alloc reuses this slot. */
            if (i < self->count)
                self->count = i;
            return;
        }
    }
    /*  Not one of our objects; the freelist is preserved. */
}
