#ifndef MB_WORKER_H_INCLUDED
#define MB_WORKER_H_INCLUDED

#include "evloop.h"
#include "timerset.h"
#include "fsm.h"
#include "../utils/queue.h"
#include "../utils/cont.h"
#include "../pal/thread.h"
#include "../pal/efd.h"
#include "../pal/mutex.h"
#include "../pal/atomic.h"

struct mb_worker_task {
    int src;
    struct mb_fsm *owner;
    struct mb_queue_item item;
};

void mb_worker_task_init (struct mb_worker_task *self, int src,
    struct mb_fsm *owner);
void mb_worker_task_term (struct mb_worker_task *self);

struct mb_worker {
    struct mb_thread thread;
    struct mb_evloop evloop;
    struct mb_timerset timerset;
    struct mb_efd efd;
    struct mb_queue tasks;
    struct mb_mutex tasks_sync;
    mb_atomic_int running;
};

int mb_worker_init (struct mb_worker *self);
void mb_worker_term (struct mb_worker *self);
void mb_worker_execute (struct mb_worker *self,
    struct mb_worker_task *task);

#define MB_WORKER_TASK_EXECUTE 1

#endif
