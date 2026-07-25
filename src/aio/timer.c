#include "timer.h"
#include "timerset.h"
#include "../utils/alloc.h"

#include <stdlib.h>

static void mb_timer_handle_expired (struct mb_timerset_hndl *hndl)
{
    struct mb_timer *timer = hndl->timer;
    timer->done.src = 1;
    mb_fsm_raise (&timer->fsm, &timer->done, MB_TIMER_DONE);
}

static void mb_timer_handler (struct mb_fsm *self, int src, int type,
    void *srcptr)
{
    struct mb_timer *timer = (struct mb_timer *) self;
    (void) srcptr;

    switch (timer->state) {
    case 0:
        switch (src) {
        case MB_FSM_ACTION:
            switch (type) {
            case MB_FSM_START:
                if (timer->timerset) {
                    timer->hndl->timeout = timer->timeout;
                    mb_timerset_insert (timer->timerset, timer->hndl);
                }
                timer->state = 1;
                return;
            default:
                return;
            }
        default:
            return;
        }
    case 1:
        switch (src) {
        case MB_FSM_ACTION:
            switch (type) {
            case MB_FSM_STOP:
                if (timer->hndl)
                    mb_timerset_cancel (timer->hndl);
                timer->state = 0;
                return;
            default:
                return;
            }
        case 1:
            timer->state = 0;
            mb_fsm_raise (self, &timer->done, MB_TIMER_DONE);
            return;
        default:
            return;
        }
    }
}

static void mb_timer_shutdown (struct mb_fsm *self, int src, int type,
    void *srcptr)
{
    struct mb_timer *timer = (struct mb_timer *) self;
    if (timer->state == 1)
        mb_timerset_cancel (timer->hndl);
    mb_timer_handler (self, src, type, srcptr);
}

void mb_timer_init (struct mb_timer *self, int src,
    struct mb_fsm *owner)
{
    mb_fsm_init (&self->fsm, mb_timer_handler, mb_timer_shutdown,
        src, NULL, owner);
    self->state = 0;
    self->hndl = (struct mb_timerset_hndl *) mb_alloc (
        sizeof (*self->hndl));
    mb_alloc_assert (self->hndl);
    self->hndl->timeout = 0;
    self->hndl->expiry = 0;
    self->hndl->set = NULL;
    self->hndl->prev = NULL;
    self->hndl->next = NULL;
    self->hndl->fn = mb_timer_handle_expired;
    self->hndl->timer = self;
    self->timerset = NULL;
    self->timeout = 0;
    mb_fsm_event_init (&self->done);
}

void mb_timer_term (struct mb_timer *self)
{
    if (self->hndl && self->hndl->set)
        mb_timerset_cancel (self->hndl);
    mb_fsm_event_term (&self->done);
    mb_fsm_term (&self->fsm);
    mb_free (self->hndl);
}

void mb_timer_set_timerset (struct mb_timer *self, struct mb_timerset *timerset)
{
    self->timerset = timerset;
}

void mb_timer_start (struct mb_timer *self, int timeout)
{
    self->timeout = timeout;
    mb_fsm_start (&self->fsm);
}

void mb_timer_stop (struct mb_timer *self)
{
    mb_fsm_stop (&self->fsm);
}
