#include "mutex.h"

#if defined _WIN32
#include "win.h"
#else
#include <pthread.h>
#endif

void mb_mutex_init (struct mb_mutex *self)
{
#if defined _WIN32
    InitializeSRWLock (&self->srwlock);
#else
    pthread_mutex_init (&self->mutex, NULL);
#endif
}

void mb_mutex_term (struct mb_mutex *self)
{
#if !defined _WIN32
    pthread_mutex_destroy (&self->mutex);
#endif
}

void mb_mutex_lock (struct mb_mutex *self)
{
#if defined _WIN32
    AcquireSRWLockExclusive (&self->srwlock);
#else
    pthread_mutex_lock (&self->mutex);
#endif
}

void mb_mutex_unlock (struct mb_mutex *self)
{
#if defined _WIN32
    ReleaseSRWLockExclusive (&self->srwlock);
#else
    pthread_mutex_unlock (&self->mutex);
#endif
}
