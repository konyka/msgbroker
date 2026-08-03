#include "ctx.h"
#include "fsm.h"
#include "pool.h"
#include "../utils/fast.h"

#include <stddef.h>

void mb_ctx_init (struct mb_ctx *self, struct mb_pool *pool,
    mb_ctx_onleave onleave)
{
    mb_mutex_init (&self->sync);
    self->pool = pool;
    mb_queue_init (&self->events);
    mb_queue_init (&self->eventsto);
    self->onleave = onleave;
    mb_atomic_store (&self->terminated, 0);
}

void mb_ctx_term (struct mb_ctx *self)
{
    /*  Mark terminated before tearing down the queues so a concurrent
     *  mb_ctx_raise sees the flag and drops the event rather than
     *  pushing into a destroyed queue (use-after-free). */
    mb_atomic_store (&self->terminated, 1);
    mb_queue_term (&self->events);
    mb_queue_term (&self->eventsto);
    mb_mutex_term (&self->sync);
}

void mb_ctx_enter (struct mb_ctx *self)
{
    mb_mutex_lock (&self->sync);
}

void mb_ctx_leave (struct mb_ctx *self)
{
    if (self->onleave)
        self->onleave (self);
    mb_mutex_unlock (&self->sync);
}

void mb_ctx_raise (struct mb_ctx *self, struct mb_fsm_event *event)
{
    if (mb_atomic_load (&self->terminated))
        return;
    mb_queue_push (&self->events, &event->item);
}

void mb_ctx_raiseto (struct mb_ctx *self, struct mb_fsm_event *event)
{
    if (mb_atomic_load (&self->terminated))
        return;
    mb_queue_push (&self->eventsto, &event->item);
}
