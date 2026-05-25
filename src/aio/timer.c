#include "timer.h"
#include "timerset.h"
#include "../pal/clock.h"

#include <stdlib.h>

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
    self->hndl = NULL;
    self->timeout = 0;
    mb_fsm_event_init (&self->done);
}

void mb_timer_term (struct mb_timer *self)
{
    mb_fsm_event_term (&self->done);
    mb_fsm_term (&self->fsm);
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
