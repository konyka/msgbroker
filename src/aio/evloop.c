#include "evloop.h"

#if defined _WIN32
#include "../pal/win.h"
#elif defined __linux__
#include <sys/epoll.h>
#include <poll.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#elif defined MB_HAVE_KQUEUE
#include <sys/event.h>
#include <unistd.h>
#include <stdlib.h>
#else
#include <poll.h>
#include <stdlib.h>
#endif

/* ── Windows (IOCP) ─────────────────────────────────────────────── */

#if defined _WIN32

int mb_evloop_init (struct mb_evloop *self)
{
    self->iocp = CreateIoCompletionPort (INVALID_HANDLE_VALUE, NULL, 0, 0);
    return self->iocp ? 0 : -1;
}

void mb_evloop_term (struct mb_evloop *self)
{
    CloseHandle (self->iocp);
}

int mb_evloop_add (struct mb_evloop *self, int fd, int events,
    struct mb_evloop_cb *cb)
{
    (void) self; (void) fd; (void) events; (void) cb;
    return 0;
}

int mb_evloop_modify (struct mb_evloop *self, int fd, int events,
    struct mb_evloop_cb *cb)
{
    (void) self; (void) fd; (void) events; (void) cb;
    return 0;
}

int mb_evloop_remove (struct mb_evloop *self, int fd)
{
    (void) self; (void) fd;
    return 0;
}

int mb_evloop_poll (struct mb_evloop *self, int timeout_ms)
{
    (void) self; (void) timeout_ms;
    return 0;
}

const char *mb_evloop_backend_name (struct mb_evloop *self)
{
    (void) self;
    return "iocp";
}

/* ── Linux (io_uring + epoll fallback) ──────────────────────────── */

#elif defined __linux__

static int mb_evloop_init_iouring (struct mb_evloop *self)
{
    int rc = io_uring_queue_init (64, &self->iouring.ring, 0);
    if (rc < 0)
        return rc;
    self->iouring.ring_fd = self->iouring.ring.ring_fd;
    self->backend = MB_EVLOOP_BACKEND_IOURING;
    return 0;
}

static int mb_evloop_init_epoll (struct mb_evloop *self)
{
    self->ep.epoll_fd = epoll_create1 (EPOLL_CLOEXEC);
    if (self->ep.epoll_fd < 0)
        return -errno;
    self->backend = MB_EVLOOP_BACKEND_EPOLL;
    return 0;
}

int mb_evloop_init (struct mb_evloop *self)
{
    memset (self, 0, sizeof (*self));
    self->backend = MB_EVLOOP_BACKEND_EPOLL;

    if (mb_evloop_init_iouring (self) == 0)
        return 0;

    return mb_evloop_init_epoll (self);
}

void mb_evloop_term (struct mb_evloop *self)
{
    if (self->backend == MB_EVLOOP_BACKEND_IOURING)
        io_uring_queue_exit (&self->iouring.ring);
    else
        close (self->ep.epoll_fd);
}

static int mb_evloop_epoll_mask (int events)
{
    int mask = 0;
    if (events & MB_EVLOOP_IN) mask |= EPOLLIN;
    if (events & MB_EVLOOP_OUT) mask |= EPOLLOUT;
    return mask;
}

static int mb_evloop_uring_mask (int events)
{
    int mask = 0;
    if (events & MB_EVLOOP_IN) mask |= POLLIN;
    if (events & MB_EVLOOP_OUT) mask |= POLLOUT;
    return mask;
}

int mb_evloop_add (struct mb_evloop *self, int fd, int events,
    struct mb_evloop_cb *cb)
{
    if (self->backend == MB_EVLOOP_BACKEND_IOURING) {
        struct io_uring_sqe *sqe = io_uring_get_sqe (&self->iouring.ring);
        if (!sqe)
            return -ENOMEM;
        io_uring_prep_poll_add (sqe, fd, mb_evloop_uring_mask (events));
        io_uring_sqe_set_data (sqe, cb);
        io_uring_submit (&self->iouring.ring);
        return 0;
    }

    struct epoll_event ev;
    ev.events = mb_evloop_epoll_mask (events);
    ev.data.ptr = cb;
    return epoll_ctl (self->ep.epoll_fd, EPOLL_CTL_ADD, fd, &ev);
}

int mb_evloop_modify (struct mb_evloop *self, int fd, int events,
    struct mb_evloop_cb *cb)
{
    if (self->backend == MB_EVLOOP_BACKEND_IOURING) {
        struct io_uring_sqe *sqe;
        struct io_uring_cqe *cqe;

        sqe = io_uring_get_sqe (&self->iouring.ring);
        if (!sqe)
            return -ENOMEM;
        io_uring_prep_poll_remove (sqe, (uint64_t)(uintptr_t) cb);
        io_uring_submit (&self->iouring.ring);
        io_uring_wait_cqe (&self->iouring.ring, &cqe);
        io_uring_cqe_seen (&self->iouring.ring, cqe);

        sqe = io_uring_get_sqe (&self->iouring.ring);
        if (!sqe)
            return -ENOMEM;
        io_uring_prep_poll_add (sqe, fd, mb_evloop_uring_mask (events));
        io_uring_sqe_set_data (sqe, cb);
        io_uring_submit (&self->iouring.ring);
        return 0;
    }

    struct epoll_event ev;
    ev.events = mb_evloop_epoll_mask (events);
    ev.data.ptr = cb;
    return epoll_ctl (self->ep.epoll_fd, EPOLL_CTL_MOD, fd, &ev);
}

int mb_evloop_remove (struct mb_evloop *self, int fd)
{
    if (self->backend == MB_EVLOOP_BACKEND_IOURING) {
        (void) fd;
        return 0;
    }
    return epoll_ctl (self->ep.epoll_fd, EPOLL_CTL_DEL, fd, NULL);
}

int mb_evloop_poll (struct mb_evloop *self, int timeout_ms)
{
    if (self->backend == MB_EVLOOP_BACKEND_IOURING) {
        struct __kernel_timespec ts;
        struct io_uring_cqe *cqe;
        int rc;

        if (timeout_ms >= 0) {
            ts.tv_sec = timeout_ms / 1000;
            ts.tv_nsec = (long)(timeout_ms % 1000) * 1000000L;
            rc = io_uring_wait_cqe_timeout (&self->iouring.ring, &cqe, &ts);
        } else {
            rc = io_uring_wait_cqe (&self->iouring.ring, &cqe);
        }

        if (rc < 0)
            return (rc == -ETIME) ? 0 : rc;

        struct mb_evloop_cb *cb =
            (struct mb_evloop_cb *) io_uring_cqe_get_data (cqe);
        int ev_flags = 0;
        if (cqe->res & (POLLIN | POLLHUP | POLLERR))
            ev_flags |= MB_EVLOOP_IN;
        if (cqe->res & POLLOUT)
            ev_flags |= MB_EVLOOP_OUT;
        io_uring_cqe_seen (&self->iouring.ring, cqe);

        if (cb && cb->on_event)
            cb->on_event (cb->data, ev_flags);

        if (ev_flags && cb) {
            struct io_uring_sqe *sqe =
                io_uring_get_sqe (&self->iouring.ring);
            if (sqe && cqe->flags == 0) {
                io_uring_prep_poll_add (sqe, cqe->res >> 16,
                    POLLIN | POLLOUT);
            }
        }

        return 1;
    }

    struct epoll_event events [32];
    int n = epoll_wait (self->ep.epoll_fd, events, 32, timeout_ms);
    if (n <= 0)
        return n;
    int i;
    for (i = 0; i < n; i++) {
        struct mb_evloop_cb *cb =
            (struct mb_evloop_cb *) events[i].data.ptr;
        int ev_flags = 0;
        if (events[i].events & (EPOLLIN | EPOLLHUP | EPOLLERR))
            ev_flags |= MB_EVLOOP_IN;
        if (events[i].events & EPOLLOUT)
            ev_flags |= MB_EVLOOP_OUT;
        if (cb && cb->on_event)
            cb->on_event (cb->data, ev_flags);
    }
    return n;
}

const char *mb_evloop_backend_name (struct mb_evloop *self)
{
    if (self->backend == MB_EVLOOP_BACKEND_IOURING)
        return "io_uring";
    return "epoll";
}

/* ── kqueue (macOS/BSD) ─────────────────────────────────────────── */

#elif defined MB_HAVE_KQUEUE

int mb_evloop_init (struct mb_evloop *self)
{
    self->kqueue_fd = kqueue ();
    return self->kqueue_fd >= 0 ? 0 : -1;
}

void mb_evloop_term (struct mb_evloop *self)
{
    close (self->kqueue_fd);
}

int mb_evloop_add (struct mb_evloop *self, int fd, int events,
    struct mb_evloop_cb *cb)
{
    (void) self; (void) fd; (void) events; (void) cb;
    return 0;
}

int mb_evloop_modify (struct mb_evloop *self, int fd, int events,
    struct mb_evloop_cb *cb)
{
    (void) self; (void) fd; (void) events; (void) cb;
    return 0;
}

int mb_evloop_remove (struct mb_evloop *self, int fd)
{
    (void) self; (void) fd;
    return 0;
}

int mb_evloop_poll (struct mb_evloop *self, int timeout_ms)
{
    (void) self; (void) timeout_ms;
    return 0;
}

const char *mb_evloop_backend_name (struct mb_evloop *self)
{
    (void) self;
    return "kqueue";
}

/* ── poll (generic fallback) ─────────────────────────────────────── */

#else

int mb_evloop_init (struct mb_evloop *self)
{
    (void) self;
    return 0;
}

void mb_evloop_term (struct mb_evloop *self)
{
    (void) self;
}

int mb_evloop_add (struct mb_evloop *self, int fd, int events,
    struct mb_evloop_cb *cb)
{
    (void) self; (void) fd; (void) events; (void) cb;
    return 0;
}

int mb_evloop_modify (struct mb_evloop *self, int fd, int events,
    struct mb_evloop_cb *cb)
{
    (void) self; (void) fd; (void) events; (void) cb;
    return 0;
}

int mb_evloop_remove (struct mb_evloop *self, int fd)
{
    (void) self; (void) fd;
    return 0;
}

int mb_evloop_poll (struct mb_evloop *self, int timeout_ms)
{
    (void) self; (void) timeout_ms;
    return 0;
}

const char *mb_evloop_backend_name (struct mb_evloop *self)
{
    (void) self;
    return "poll";
}

#endif
