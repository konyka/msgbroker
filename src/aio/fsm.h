#ifndef MB_FSM_H_INCLUDED
#define MB_FSM_H_INCLUDED

#include "../utils/queue.h"

struct mb_ctx;
struct mb_worker;

struct mb_fsm_event {
    struct mb_fsm *fsm;
    int src;
    void *srcptr;
    int type;
    struct mb_queue_item item;
};

#define MB_FSM_ACTION -2
#define MB_FSM_START  -2
#define MB_FSM_STOP   -3

typedef void (*mb_fsm_fn) (struct mb_fsm *self, int src, int type,
    void *srcptr);

struct mb_fsm {
    mb_fsm_fn fn;
    mb_fsm_fn shutdown_fn;
    int state;
    int src;
    void *srcptr;
    struct mb_fsm *owner;
    struct mb_ctx *ctx;
    struct mb_fsm_event stopped;
};

void mb_fsm_init_root (struct mb_fsm *self, mb_fsm_fn fn,
    mb_fsm_fn shutdown_fn, struct mb_ctx *ctx);
void mb_fsm_init (struct mb_fsm *self, mb_fsm_fn fn,
    mb_fsm_fn shutdown_fn, int src, void *srcptr,
    struct mb_fsm *owner);
void mb_fsm_term (struct mb_fsm *self);
void mb_fsm_start (struct mb_fsm *self);
void mb_fsm_stop (struct mb_fsm *self);
int mb_fsm_isidle (struct mb_fsm *self);
void mb_fsm_swap_owner (struct mb_fsm *self, struct mb_fsm *newowner,
    int newsrc);
void mb_fsm_event_init (struct mb_fsm_event *self);
void mb_fsm_event_term (struct mb_fsm_event *self);
int mb_fsm_event_active (struct mb_fsm_event *self);
void mb_fsm_event_process (struct mb_fsm_event *self);
void mb_fsm_event_start (struct mb_fsm_event *self, int type);
void mb_fsm_event_stop (struct mb_fsm_event *self, int type);
void mb_fsm_raise (struct mb_fsm *self, struct mb_fsm_event *event, int type);

#endif
