#include "semaphore.h"

#if defined _WIN32
#include "win.h"
#else
#include <semaphore.h>
#include <time.h>
#include <errno.h>
#endif

void mb_sem_init (struct mb_sem *self)
{
#if defined _WIN32
    self->handle = CreateSemaphore (NULL, 0, LONG_MAX, NULL);
#else
    sem_init (&self->sem, 0, 0);
#endif
}

void mb_sem_term (struct mb_sem *self)
{
#if defined _WIN32
    CloseHandle (self->handle);
#else
    sem_destroy (&self->sem);
#endif
}

void mb_sem_post (struct mb_sem *self)
{
#if defined _WIN32
    ReleaseSemaphore (self->handle, 1, NULL);
#else
    sem_post (&self->sem);
#endif
}

int mb_sem_wait (struct mb_sem *self)
{
#if defined _WIN32
    WaitForSingleObject (self->handle, INFINITE);
    return 0;
#else
    return sem_wait (&self->sem);
#endif
}

int mb_sem_timedwait (struct mb_sem *self, int timeout_ms)
{
#if defined _WIN32
    DWORD ms = (timeout_ms < 0) ? INFINITE : (DWORD) timeout_ms;
    DWORD rc = WaitForSingleObject (self->handle, ms);
    if (rc == WAIT_TIMEOUT) return -ETIMEDOUT;
    return 0;
#else
    if (timeout_ms < 0)
        return sem_wait (&self->sem);
    struct timespec ts;
    clock_gettime (CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }
    int rc = sem_timedwait (&self->sem, &ts);
    if (rc != 0 && errno == ETIMEDOUT) return -ETIMEDOUT;
    return rc;
#endif
}
