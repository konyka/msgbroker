#ifndef MB_THREADPOOL_H_INCLUDED
#define MB_THREADPOOL_H_INCLUDED

#include "worker.h"
#include "../pal/mutex.h"
#include "../pal/condvar.h"
#include "../pal/atomic.h"

struct mb_threadpool_task {
    void (*fn) (void *arg);
    void *arg;
    struct mb_queue_item item;
};

struct mb_threadpool {
    struct mb_worker *workers;
    int nworkers;
    struct mb_queue global_queue;
    struct mb_mutex global_lock;
    struct mb_condvar wait_cond;
    mb_atomic_int running;
    mb_atomic_int pending;
};

int mb_threadpool_init (struct mb_threadpool *self, int nworkers);
void mb_threadpool_term (struct mb_threadpool *self);
void mb_threadpool_submit (struct mb_threadpool *self,
    struct mb_threadpool_task *task);
void mb_threadpool_wait (struct mb_threadpool *self);

#endif
