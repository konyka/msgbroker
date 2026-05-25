#include "threadpool.h"
#include "../utils/alloc.h"
#include "../utils/cont.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct mb_threadpool_thread {
    struct mb_thread thread;
    struct mb_threadpool *pool;
    struct mb_queue local_queue;
    struct mb_mutex local_lock;
    int running;
};

static void mb_threadpool_worker_fn (void *arg)
{
    struct mb_threadpool_thread *t = (struct mb_threadpool_thread *) arg;
    struct mb_threadpool *pool = t->pool;

    while (t->running) {
        mb_mutex_lock (&t->local_lock);
        struct mb_queue_item *qi = mb_queue_pop (&t->local_queue);
        mb_mutex_unlock (&t->local_lock);

        if (!qi) {
            struct timespec ts = {0, 1000000};
            nanosleep (&ts, NULL);
            continue;
        }

        struct mb_threadpool_task *task =
            mb_cont (qi, struct mb_threadpool_task, item);
        task->fn (task->arg);
        mb_atomic_fetch_sub (&pool->pending, 1);
        mb_condvar_signal (&pool->wait_cond);
    }
}

struct mb_threadpool_internal {
    struct mb_threadpool_thread *threads;
};

int mb_threadpool_init (struct mb_threadpool *self, int nworkers)
{
    int i;
    if (nworkers <= 0)
        nworkers = 4;

    self->nworkers = nworkers;
    struct mb_threadpool_thread *threads =
        (struct mb_threadpool_thread *)
        mb_alloc (nworkers * sizeof (struct mb_threadpool_thread));
    if (!threads)
        return -ENOMEM;

    mb_mutex_init (&self->global_lock);
    mb_condvar_init (&self->wait_cond);
    mb_atomic_store (&self->running, 1);
    mb_atomic_store (&self->pending, 0);

    for (i = 0; i < nworkers; i++) {
        threads[i].pool = self;
        threads[i].running = 1;
        mb_queue_init (&threads[i].local_queue);
        mb_mutex_init (&threads[i].local_lock);
        mb_thread_init (&threads[i].thread);
        mb_thread_start (&threads[i].thread,
            mb_threadpool_worker_fn, &threads[i]);
    }

    self->workers = (struct mb_worker *) threads;
    return 0;
}

void mb_threadpool_term (struct mb_threadpool *self)
{
    int i;
    struct mb_threadpool_thread *threads =
        (struct mb_threadpool_thread *) self->workers;
    for (i = 0; i < self->nworkers; i++) {
        threads[i].running = 0;
        mb_thread_join (&threads[i].thread);
        mb_thread_term (&threads[i].thread);
        mb_mutex_term (&threads[i].local_lock);
        mb_queue_term (&threads[i].local_queue);
    }
    mb_condvar_term (&self->wait_cond);
    mb_mutex_term (&self->global_lock);
    mb_free (threads);
    self->workers = NULL;
}

void mb_threadpool_submit (struct mb_threadpool *self,
    struct mb_threadpool_task *task)
{
    struct mb_threadpool_thread *threads =
        (struct mb_threadpool_thread *) self->workers;
    mb_atomic_fetch_add (&self->pending, 1);
    static mb_atomic_int round_robin = 0;
    int idx = mb_atomic_fetch_add (&round_robin, 1) % self->nworkers;
    mb_mutex_lock (&threads[idx].local_lock);
    mb_queue_push (&threads[idx].local_queue, &task->item);
    mb_mutex_unlock (&threads[idx].local_lock);
}

void mb_threadpool_wait (struct mb_threadpool *self)
{
    while (mb_atomic_load (&self->pending) > 0) {
        mb_mutex_lock (&self->global_lock);
        mb_condvar_wait (&self->wait_cond, &self->global_lock, 100);
        mb_mutex_unlock (&self->global_lock);
    }
}
