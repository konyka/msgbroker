#include "slab.h"
#include "../utils/alloc.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void mb_slab_init (struct mb_slab *self, size_t obj_size, size_t capacity)
{
    size_t i;
    void **freelist;

    self->obj_size = obj_size;
    self->count = 0;
    self->capacity = 0;
    self->freelist = NULL;

    if (capacity == 0 || obj_size == 0)
        return;
    if (capacity > SIZE_MAX / sizeof (void *))
        return;

    freelist = (void **) mb_alloc (capacity * sizeof (void *));
    if (!freelist)
        return;

    for (i = 0; i < capacity; i++) {
        void *obj = mb_alloc (obj_size);
        if (!obj) {
            if (i == 0) {
                mb_free (freelist);
                return;
            }
            /* Keep the successfully allocated prefix as a smaller slab. */
            self->freelist = freelist;
            self->capacity = i;
            return;
        }
        memset (obj, 0, obj_size);
        freelist[i] = obj;
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

void *mb_slab_alloc (struct mb_slab *self)
{
    void *obj;

    if (!self->freelist || self->count >= self->capacity)
        return NULL;
    obj = self->freelist[self->count];
    if (!obj)
        return NULL;
    self->freelist[self->count] = NULL;
    self->count++;
    return obj;
}

void mb_slab_free (struct mb_slab *self, void *obj)
{
    if (!obj || !self->freelist || self->count == 0)
        return;
    self->count--;
    self->freelist[self->count] = obj;
}
