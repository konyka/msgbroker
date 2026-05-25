#include "timerset.h"
#include "../pal/clock.h"

#include <stddef.h>
#include <stdlib.h>

struct mb_timerset {
    struct mb_timerset_hndl *head;
};

void mb_timerset_init (struct mb_timerset *self)
{
    self->head = NULL;
}

void mb_timerset_term (struct mb_timerset *self)
{
    self->head = NULL;
}

int mb_timerset_timeout (struct mb_timerset *self)
{
    if (!self->head)
        return -1;
    uint64_t now = mb_clock_ms ();
    int remaining = (int) (self->head->expiry - now);
    return remaining > 0 ? remaining : 0;
}

static void mb_timerset_insert_sorted (struct mb_timerset *self,
    struct mb_timerset_hndl *hndl)
{
    struct mb_timerset_hndl **pp = &self->head;
    while (*pp && (*pp)->expiry <= hndl->expiry)
        pp = &(*pp)->next;
    hndl->next = *pp;
    hndl->prev = pp == &self->head ? NULL :
        (struct mb_timerset_hndl *) ((char *) pp - offsetof (struct mb_timerset_hndl, next));
    if (*pp)
        (*pp)->prev = hndl;
    *pp = hndl;
}

void mb_timerset_insert (struct mb_timerset *self,
    struct mb_timerset_hndl *hndl)
{
    hndl->expiry = mb_clock_ms () + (uint64_t) hndl->timeout;
    mb_timerset_insert_sorted (self, hndl);
}

void mb_timerset_cancel (struct mb_timerset_hndl *hndl)
{
    if (hndl->prev)
        hndl->prev->next = hndl->next;
    if (hndl->next)
        hndl->next->prev = hndl->prev;
    hndl->prev = NULL;
    hndl->next = NULL;
}

void mb_timerset_tick (struct mb_timerset *self)
{
    (void) self;
}
