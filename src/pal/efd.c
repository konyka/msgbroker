#include "efd.h"
#include "../utils/fast.h"

#if defined _WIN32
#include "win.h"
#elif defined MB_HAVE_EVENTFD
#include <sys/eventfd.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#endif

void mb_efd_init (struct mb_efd *self)
{
#if defined _WIN32
    self->event = CreateEvent (NULL, TRUE, FALSE, NULL);
#elif defined MB_HAVE_EVENTFD
    self->fd = eventfd (0, EFD_NONBLOCK | EFD_CLOEXEC);
#else
    int rc = pipe (self->fds);
    (void) rc;
    fcntl (self->fds [0], F_SETFL, O_NONBLOCK);
    fcntl (self->fds [1], F_SETFL, O_NONBLOCK);
#endif
}

void mb_efd_term (struct mb_efd *self)
{
#if defined _WIN32
    CloseHandle (self->event);
#elif defined MB_HAVE_EVENTFD
    close (self->fd);
#else
    close (self->fds [0]);
    close (self->fds [1]);
#endif
}

int mb_efd_getfd (struct mb_efd *self)
{
#if defined _WIN32
    return -1;
#elif defined MB_HAVE_EVENTFD
    return self->fd;
#else
    return self->fds [0];
#endif
}

void mb_efd_signal (struct mb_efd *self)
{
#if defined _WIN32
    SetEvent (self->event);
#elif defined MB_HAVE_EVENTFD
    uint64_t val = 1;
    write (self->fd, &val, sizeof (val));
#else
    char c = 1;
    write (self->fds [1], &c, 1);
#endif
}

void mb_efd_unsignal (struct mb_efd *self)
{
#if defined _WIN32
    ResetEvent (self->event);
#elif defined MB_HAVE_EVENTFD
    uint64_t val;
    read (self->fd, &val, sizeof (val));
#else
    char buf [16];
    while (read (self->fds [0], buf, sizeof (buf)) > 0) {}
#endif
}

int mb_efd_wait (struct mb_efd *self, int timeout_ms)
{
#if defined _WIN32
    DWORD ms = (timeout_ms < 0) ? INFINITE : (DWORD) timeout_ms;
    return WaitForSingleObject (self->event, ms) == WAIT_OBJECT_0 ? 0 : -1;
#elif defined MB_HAVE_EVENTFD
    struct pollfd pfd;
    pfd.fd = self->fd;
    pfd.events = POLLIN;
    int rc = poll (&pfd, 1, timeout_ms);
    return rc > 0 ? 0 : -1;
#else
    struct pollfd pfd;
    pfd.fd = self->fds [0];
    pfd.events = POLLIN;
    int rc = poll (&pfd, 1, timeout_ms);
    return rc > 0 ? 0 : -1;
#endif
}
