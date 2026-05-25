#include "condvar.h"

#if defined _WIN32
#include "win.h"
#else
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#endif

void mb_condvar_init (struct mb_condvar *self)
{
#if defined _WIN32
    InitializeConditionVariable (&self->cv);
#else
    pthread_cond_init (&self->cond, NULL);
#endif
}

void mb_condvar_term (struct mb_condvar *self)
{
#if !defined _WIN32
    pthread_cond_destroy (&self->cond);
#endif
}

void mb_condvar_signal (struct mb_condvar *self)
{
#if defined _WIN32
    WakeConditionVariable (&self->cv);
#else
    pthread_cond_signal (&self->cond);
#endif
}

void mb_condvar_broadcast (struct mb_condvar *self)
{
#if defined _WIN32
    WakeAllConditionVariable (&self->cv);
#else
    pthread_cond_broadcast (&self->cond);
#endif
}

int mb_condvar_wait (struct mb_condvar *self, struct mb_mutex *mtx, int timeout_ms)
{
#if defined _WIN32
    DWORD ms = (timeout_ms < 0) ? INFINITE : (DWORD) timeout_ms;
    if (!SleepConditionVariableSRW (&self->cv, &mtx->srwlock, ms, 0))
        return ETIMEDOUT;
    return 0;
#else
    if (timeout_ms < 0) {
        return pthread_cond_wait (&self->cond, &mtx->mutex);
    } else {
        struct timespec ts;
        clock_gettime (CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000L;
        }
        return pthread_cond_timedwait (&self->cond, &mtx->mutex, &ts);
    }
#endif
}
