#ifndef MB_TIMER_H_INCLUDED
#define MB_TIMER_H_INCLUDED

#include "fsm.h"

struct mb_worker_timer;
struct mb_timerset_hndl;

struct mb_timer {
    struct mb_fsm fsm;
    int state;
    struct mb_timerset_hndl *hndl;
    int timeout;
    struct mb_fsm_event done;
};

void mb_timer_init (struct mb_timer *self, int src,
    struct mb_fsm *owner);
void mb_timer_term (struct mb_timer *self);
void mb_timer_start (struct mb_timer *self, int timeout);
void mb_timer_stop (struct mb_timer *self);

#define MB_TIMER_DONE 1

#endif
