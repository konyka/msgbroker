#include "pool.h"
#include "../pal/thread.h"
#include "../utils/cont.h"
#include "../pal/atomic.h"

static void mb_pool_worker_routine (void *arg)
{
    struct mb_worker *w = (struct mb_worker *) arg;
    while (mb_atomic_load (&w->running)) {
        mb_evloop_poll (&w->evloop, 100);
        mb_timerset_tick (&w->timerset);
        mb_mutex_lock (&w->tasks_sync);
        while (!mb_queue_empty (&w->tasks)) {
            struct mb_queue_item *qi = mb_queue_pop (&w->tasks);
            struct mb_worker_task *task =
                mb_cont (qi, struct mb_worker_task, item);
            task->owner->fn (task->owner, task->src,
                MB_WORKER_TASK_EXECUTE, NULL);
        }
        mb_mutex_unlock (&w->tasks_sync);
    }
}

int mb_pool_init (struct mb_pool *self)
{
    int rc = mb_worker_init (&self->worker);
    if (rc != 0)
        return rc;
    self->started = 0;
    mb_atomic_store (&self->worker.running, 1);
    rc = mb_thread_start (&self->worker.thread,
        mb_pool_worker_routine, &self->worker);
    if (rc != 0)
        return rc;
    self->started = 1;
    return 0;
}

void mb_pool_term (struct mb_pool *self)
{
    if (self->started) {
        mb_atomic_store (&self->worker.running, 0);
        mb_efd_signal (&self->worker.efd);
        mb_thread_join (&self->worker.thread);
    }
    mb_worker_term (&self->worker);
}

struct mb_worker *mb_pool_choose_worker (struct mb_pool *self)
{
    return &self->worker;
}
