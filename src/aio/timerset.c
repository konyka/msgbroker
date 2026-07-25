#include "timerset.h"
#include "../pal/clock.h"

#include <stdint.h>

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
    struct mb_timerset_hndl *it = self->head;
    struct mb_timerset_hndl *prev = NULL;

    hndl->set = self;
    hndl->prev = NULL;
    hndl->next = NULL;

    while (it && it->expiry <= hndl->expiry) {
        prev = it;
        it = it->next;
    }

    hndl->prev = prev;
    hndl->next = it;
    if (prev)
        prev->next = hndl;
    else
        self->head = hndl;
    if (it)
        it->prev = hndl;
}

void mb_timerset_insert (struct mb_timerset *self,
    struct mb_timerset_hndl *hndl)
{
    if (hndl->set)
        mb_timerset_cancel (hndl);
    hndl->expiry = mb_clock_ms () + (uint64_t) hndl->timeout;
    mb_timerset_insert_sorted (self, hndl);
}

void mb_timerset_cancel (struct mb_timerset_hndl *hndl)
{
    if (!hndl->set)
        return;

    if (hndl->prev)
        hndl->prev->next = hndl->next;
    else
        hndl->set->head = hndl->next;
    if (hndl->next)
        hndl->next->prev = hndl->prev;
    hndl->set = NULL;
    hndl->prev = NULL;
    hndl->next = NULL;
}

void mb_timerset_tick (struct mb_timerset *self)
{
    uint64_t now = mb_clock_ms ();

    while (self->head && self->head->expiry <= now) {
        struct mb_timerset_hndl *hndl = self->head;
        mb_timerset_cancel (hndl);
        if (hndl->fn)
            hndl->fn (hndl);
    }
}
