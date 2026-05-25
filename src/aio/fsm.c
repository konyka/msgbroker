#include "fsm.h"
#include "ctx.h"
#include "../utils/queue.h"
#include "../utils/fast.h"

#include <stddef.h>

void mb_fsm_init_root (struct mb_fsm *self, mb_fsm_fn fn,
    mb_fsm_fn shutdown_fn, struct mb_ctx *ctx)
{
    self->fn = fn;
    self->shutdown_fn = shutdown_fn;
    self->state = 0;
    self->src = MB_FSM_ACTION;
    self->srcptr = NULL;
    self->owner = NULL;
    self->ctx = ctx;
    mb_fsm_event_init (&self->stopped);
}

void mb_fsm_init (struct mb_fsm *self, mb_fsm_fn fn,
    mb_fsm_fn shutdown_fn, int src, void *srcptr,
    struct mb_fsm *owner)
{
    self->fn = fn;
    self->shutdown_fn = shutdown_fn;
    self->state = 0;
    self->src = src;
    self->srcptr = srcptr;
    self->owner = owner;
    self->ctx = owner->ctx;
    mb_fsm_event_init (&self->stopped);
}

void mb_fsm_term (struct mb_fsm *self)
{
    mb_fsm_event_term (&self->stopped);
}

void mb_fsm_start (struct mb_fsm *self)
{
    self->fn (self, MB_FSM_ACTION, MB_FSM_START, NULL);
}

void mb_fsm_stop (struct mb_fsm *self)
{
    if (self->shutdown_fn)
        self->shutdown_fn (self, MB_FSM_ACTION, MB_FSM_STOP, NULL);
    else
        self->fn (self, MB_FSM_ACTION, MB_FSM_STOP, NULL);
}

int mb_fsm_isidle (struct mb_fsm *self)
{
    return self->state == 0;
}

void mb_fsm_swap_owner (struct mb_fsm *self, struct mb_fsm *newowner,
    int newsrc)
{
    self->owner = newowner;
    self->src = newsrc;
}

void mb_fsm_event_init (struct mb_fsm_event *self)
{
    self->fsm = NULL;
    self->src = 0;
    self->srcptr = NULL;
    self->type = 0;
    mb_queue_item_init (&self->item);
}

void mb_fsm_event_term (struct mb_fsm_event *self)
{
    mb_queue_item_term (&self->item);
}

int mb_fsm_event_active (struct mb_fsm_event *self)
{
    return mb_queue_item_isinqueue (&self->item);
}

void mb_fsm_event_process (struct mb_fsm_event *self)
{
    self->fsm->fn (self->fsm, self->src, self->type, self->srcptr);
}

void mb_fsm_event_start (struct mb_fsm_event *self, int type)
{
    (void) self; (void) type;
}

void mb_fsm_event_stop (struct mb_fsm_event *self, int type)
{
    (void) self; (void) type;
}

void mb_fsm_raise (struct mb_fsm *self, struct mb_fsm_event *event, int type)
{
    event->fsm = self;
    event->type = type;
    if (self->ctx)
        mb_ctx_raise (self->ctx, event);
    else
        mb_fsm_event_process (event);
}
