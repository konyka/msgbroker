#include "bipc.h"
#include "sipc.h"
#include "../../core/ep.h"
#include "../../core/sock.h"
#include "../../core/pipe.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/list.h"
#include "../../utils/cont.h"

#include <msgbroker/mb.h>

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <poll.h>

#define MB_BIPC_BACKLOG 10

static void mb_bipc_stop (void *p);
static void mb_bipc_destroy (void *p);
static void mb_bipc_on_session_error (void *p);
static void mb_bipc_free_zombies (struct mb_bipc *self);

static const struct mb_ep_ops mb_bipc_ops = {
    mb_bipc_stop,
    mb_bipc_destroy,
    NULL,
};

static void mb_bipc_free_zombies (struct mb_bipc *self)
{
    while (!mb_list_empty (&self->zombies)) {
        struct mb_list_item *it = mb_list_begin (&self->zombies);
        struct mb_sipc *sipc = mb_cont (it, struct mb_sipc, item);
        mb_list_erase (&self->zombies, it);
        mb_sipc_term (sipc);
        mb_free (sipc);
    }
}

static void mb_bipc_on_session_error (void *p)
{
    struct mb_bipc *self = (struct mb_bipc *) p;
    struct mb_list_item *it;
    struct mb_list_item *next;

    mb_mutex_lock (&self->lock);
    for (it = mb_list_begin (&self->sipcs); it != mb_list_end (&self->sipcs);
        it = next) {
        struct mb_sipc *sipc = mb_cont (it, struct mb_sipc, item);
        next = mb_list_next (&self->sipcs, it);
        if (!sipc->disconnected)
            continue;
        mb_list_erase (&self->sipcs, it);
        mb_sipc_stop (sipc);
        mb_list_insert (&self->zombies, &sipc->item,
            mb_list_end (&self->zombies));
    }
    mb_mutex_unlock (&self->lock);
}

static const char *mb_bipc_parse_addr (const char *addr, char *path,
    size_t pathlen)
{
    const char *sep;

    sep = strstr (addr, "://");
    if (!sep)
        return NULL;
    sep += 3;

    strncpy (path, sep, pathlen - 1);
    path[pathlen - 1] = '\0';
    return path;
}

static void mb_bipc_accept_loop (void *arg)
{
    struct mb_bipc *self = (struct mb_bipc *) arg;

    while (self->running) {
        struct pollfd pfd;
        int rc;

        mb_mutex_lock (&self->lock);
        mb_bipc_free_zombies (self);
        mb_mutex_unlock (&self->lock);

        pfd.fd = self->listen_fd;
        pfd.events = POLLIN;
        rc = poll (&pfd, 1, 100);

        if (rc <= 0)
            continue;
        if (!self->running || self->listen_fd < 0)
            continue;

        if (pfd.revents & POLLIN) {
            struct sockaddr_un client;
            socklen_t client_len = sizeof (client);
            int client_fd;
            struct mb_sipc *sipc;

            client_fd = accept (self->listen_fd,
                (struct sockaddr *) &client, &client_len);
            if (client_fd < 0)
                continue;

            fcntl (client_fd, F_SETFL,
                fcntl (client_fd, F_GETFL, 0) | O_NONBLOCK);

            sipc = (struct mb_sipc *) mb_alloc (sizeof (struct mb_sipc));
            if (!sipc) {
                close (client_fd);
                continue;
            }

            mb_sipc_create (sipc, self->ep, client_fd);
            mb_sipc_set_on_error (sipc, mb_bipc_on_session_error, self);

            mb_mutex_lock (&self->lock);
            if (mb_sipc_start (sipc) < 0) {
                mb_sipc_term (sipc);
                mb_free (sipc);
                mb_mutex_unlock (&self->lock);
                continue;
            }
            mb_list_insert (&self->sipcs, &sipc->item,
                mb_list_end (&self->sipcs));
            mb_mutex_unlock (&self->lock);
        }
    }
}

int mb_bipc_create (struct mb_ep *ep)
{
    struct mb_bipc *self;
    struct sockaddr_un sa;
    int fd;
    int rc;

    self = (struct mb_bipc *) mb_alloc (sizeof (struct mb_bipc));
    if (!self)
        return -ENOMEM;

    memset (self->path, 0, sizeof (self->path));
    mb_bipc_parse_addr (mb_ep_getaddr (ep), self->path, sizeof (self->path));

    fd = socket (AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        mb_free (self);
        return -errno;
    }

    memset (&sa, 0, sizeof (sa));
    sa.sun_family = AF_UNIX;
    snprintf (sa.sun_path, sizeof (sa.sun_path), "%s", self->path);

    unlink (self->path);

    rc = bind (fd, (struct sockaddr *) &sa, sizeof (sa));
    if (rc < 0) {
        close (fd);
        mb_free (self);
        return -errno;
    }

    rc = listen (fd, MB_BIPC_BACKLOG);
    if (rc < 0) {
        close (fd);
        unlink (self->path);
        mb_free (self);
        return -errno;
    }

    self->ep = ep;
    self->listen_fd = fd;
    mb_list_init (&self->sipcs);
    mb_list_init (&self->zombies);
    mb_mutex_init (&self->lock);
    self->running = 1;

    mb_ep_tran_setup (ep, &mb_bipc_ops, self);

    mb_thread_init (&self->accept_thread);
    if (mb_thread_start (&self->accept_thread, mb_bipc_accept_loop, self) != 0) {
        self->running = 0;
        close (self->listen_fd);
        self->listen_fd = -1;
        unlink (self->path);
        mb_mutex_term (&self->lock);
        mb_list_term (&self->sipcs);
        mb_list_term (&self->zombies);
        mb_free (self);
        return -EAGAIN;
    }

    return 0;
}

static void mb_bipc_stop (void *p)
{
    struct mb_bipc *self = (struct mb_bipc *) p;

    self->running = 0;
    if (self->listen_fd >= 0) {
        close (self->listen_fd);
        self->listen_fd = -1;
        unlink (self->path);
    }
    mb_thread_term (&self->accept_thread);

    mb_mutex_lock (&self->lock);
    while (!mb_list_empty (&self->sipcs)) {
        struct mb_list_item *it = mb_list_begin (&self->sipcs);
        struct mb_sipc *sipc = mb_cont (it, struct mb_sipc, item);
        mb_list_erase (&self->sipcs, it);
        mb_sipc_stop (sipc);
        mb_sipc_term (sipc);
        mb_free (sipc);
    }
    mb_bipc_free_zombies (self);
    mb_mutex_unlock (&self->lock);

    mb_ep_stopped (self->ep);
}

static void mb_bipc_destroy (void *p)
{
    struct mb_bipc *self = (struct mb_bipc *) p;

    self->running = 0;

    while (!mb_list_empty (&self->sipcs)) {
        struct mb_list_item *it = mb_list_begin (&self->sipcs);
        struct mb_sipc *sipc = mb_cont (it, struct mb_sipc, item);
        mb_list_erase (&self->sipcs, it);
        mb_sipc_stop (sipc);
        mb_sipc_term (sipc);
        mb_free (sipc);
    }
    mb_bipc_free_zombies (self);

    if (self->listen_fd >= 0) {
        close (self->listen_fd);
        unlink (self->path);
    }

    mb_mutex_term (&self->lock);
    mb_list_term (&self->sipcs);
    mb_list_term (&self->zombies);
    mb_free (self);
}
