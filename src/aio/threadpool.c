#include "threadpool.h"
#include "../utils/alloc.h"
#include "../utils/cont.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static void mb_threadpool_worker_fn (void *arg)
{
    struct mb_threadpool_thread *t = (struct mb_threadpool_thread *) arg;
    struct mb_threadpool *pool = t->pool;

    while (mb_atomic_load (&t->running)) {
        mb_mutex_lock (&t->local_lock);
        struct mb_queue_item *qi = mb_queue_pop (&t->local_queue);
        if (!qi) {
            /*  Sleep on the wake condvar with a 1 ms cap. Submit
             *  signals on the same condvar so an idle worker wakes
             *  within microseconds, not milliseconds. */
            if (mb_atomic_load (&t->running))
                (void) mb_condvar_wait (&t->wake_cond, &t->local_lock, 1);
            mb_mutex_unlock (&t->local_lock);
            continue;
        }
        mb_mutex_unlock (&t->local_lock);

        struct mb_threadpool_task *task =
            mb_cont (qi, struct mb_threadpool_task, item);
        task->fn (task->arg);
        mb_atomic_fetch_sub (&pool->pending, 1);
        mb_condvar_signal (&pool->wait_cond);
    }
}

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

    self->threads = threads;
    mb_mutex_init (&self->global_lock);
    mb_condvar_init (&self->wait_cond);
    mb_atomic_store (&self->running, 1);
    mb_atomic_store (&self->pending, 0);
    mb_atomic_store (&self->round_robin, 0);

    for (i = 0; i < nworkers; i++) {
        threads[i].pool = self;
        mb_atomic_store (&threads[i].running, 1);
        mb_queue_init (&threads[i].local_queue);
        mb_mutex_init (&threads[i].local_lock);
        mb_condvar_init (&threads[i].wake_cond);
        mb_thread_init (&threads[i].thread);
        int rc = mb_thread_start (&threads[i].thread,
            mb_threadpool_worker_fn, &threads[i]);
        if (rc != 0) {
            mb_atomic_store (&threads[i].running, 0);
            int j;
            for (j = 0; j < i; j++) {
                mb_atomic_store (&threads[j].running, 0);
                mb_mutex_term (&threads[j].wake_cond);
                mb_thread_join (&threads[j].thread);
                mb_thread_term (&threads[j].thread);
                mb_mutex_term (&threads[j].local_lock);
                mb_queue_term (&threads[j].local_queue);
            }
            mb_condvar_term (&threads[i].wake_cond);
            mb_thread_term (&threads[i].thread);
            mb_mutex_term (&threads[i].local_lock);
            mb_queue_term (&threads[i].local_queue);
            mb_free (threads);
            self->threads = NULL;
            return rc;
        }
    }
    return 0;
}

void mb_threadpool_term (struct mb_threadpool *self)
{
    int i;
    struct mb_threadpool_thread *threads =
        (struct mb_threadpool_thread *) self->threads;

    if (!threads)
        return;

    for (i = 0; i < self->nworkers; i++) {
        mb_mutex_lock (&threads[i].local_lock);
        mb_atomic_store (&threads[i].running, 0);
        mb_condvar_broadcast (&threads[i].wake_cond);
        mb_mutex_unlock (&threads[i].local_lock);
    }
    for (i = 0; i < self->nworkers; i++) {
        mb_thread_join (&threads[i].thread);
        mb_thread_term (&threads[i].thread);
        mb_condvar_term (&threads[i].wake_cond);
        mb_mutex_term (&threads[i].local_lock);
        mb_queue_term (&threads[i].local_queue);
    }
    mb_condvar_term (&self->wait_cond);
    mb_mutex_term (&self->global_lock);
    mb_free (threads);
    self->threads = NULL;
}

void mb_threadpool_submit (struct mb_threadpool *self,
    struct mb_threadpool_task *task)
{
    struct mb_threadpool_thread *threads =
        (struct mb_threadpool_thread *) self->threads;
    mb_atomic_fetch_add (&self->pending, 1);
    /*  Per-instance counter (no longer process-global) so two
     *  independent pools round-robin within themselves. */
    int idx = mb_atomic_fetch_add (&self->round_robin, 1) % self->nworkers;
    mb_mutex_lock (&threads[idx].local_lock);
    mb_queue_push (&threads[idx].local_queue, &task->item);
    mb_condvar_signal (&threads[idx].wake_cond);
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
