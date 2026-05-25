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
}

void mb_ctx_term (struct mb_ctx *self)
{
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
    mb_queue_push (&self->events, &event->item);
}

void mb_ctx_raiseto (struct mb_ctx *self, struct mb_fsm_event *event)
{
    mb_queue_push (&self->eventsto, &event->item);
}
