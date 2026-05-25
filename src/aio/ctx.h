#ifndef MB_CTX_H_INCLUDED
#define MB_CTX_H_INCLUDED

#include "../pal/mutex.h"
#include "../utils/queue.h"

struct mb_pool;
struct mb_fsm_event;

struct mb_ctx;
typedef void (*mb_ctx_onleave) (struct mb_ctx *self);

struct mb_ctx {
    struct mb_mutex sync;
    struct mb_pool *pool;
    struct mb_queue events;
    struct mb_queue eventsto;
    mb_ctx_onleave onleave;
};

void mb_ctx_init (struct mb_ctx *self, struct mb_pool *pool,
    mb_ctx_onleave onleave);
void mb_ctx_term (struct mb_ctx *self);
void mb_ctx_enter (struct mb_ctx *self);
void mb_ctx_leave (struct mb_ctx *self);
void mb_ctx_raise (struct mb_ctx *self, struct mb_fsm_event *event);
void mb_ctx_raiseto (struct mb_ctx *self, struct mb_fsm_event *event);

#endif
