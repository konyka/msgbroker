#include "ctcp.h"
#include "../ipc/sipc.h"
#include "../../core/ep.h"
#include "../../core/sock.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/net.h"

#include <msgbroker/mb.h>

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

static void mb_ctcp_stop (void *p);
static void mb_ctcp_destroy (void *p);

static const struct mb_ep_ops mb_ctcp_ops = {
    mb_ctcp_stop,
    mb_ctcp_destroy,
};

static void mb_ctcp_reconnect_loop (void *arg)
{
    struct mb_ctcp *self = (struct mb_ctcp *) arg;
    int ivl = mb_ep_sock (self->ep)->reconnect_ivl;
    int ivl_max = mb_ep_sock (self->ep)->reconnect_ivl_max;
    int current_ivl = ivl;

    while (self->running) {
        int fd;
        struct mb_sipc *sipc;

        fd = mb_net_connect (self->host, self->port, NULL);
        if (fd < 0) {
            struct pollfd pfd = { .fd = -1, .events = 0 };
            poll (&pfd, 0, current_ivl);
            if (ivl_max > 0 && current_ivl < ivl_max)
                current_ivl *= 2;
            if (current_ivl > ivl_max && ivl_max > 0)
                current_ivl = ivl_max;
            continue;
        }

        fcntl (fd, F_SETFL, fcntl (fd, F_GETFL, 0) | O_NONBLOCK);

        sipc = (struct mb_sipc *) mb_alloc (sizeof (struct mb_sipc));
        if (!sipc) {
            close (fd);
            continue;
        }

        mb_sipc_create (sipc, self->ep, fd);

        mb_mutex_lock (&self->lock);
        if (!self->running) {
            mb_sipc_term (sipc);
            mb_free (sipc);
            mb_mutex_unlock (&self->lock);
            return;
        }
        self->sipc = sipc;
        mb_sipc_start (sipc);
        mb_mutex_unlock (&self->lock);
        return;
    }
}

static int mb_ctcp_do_connect (struct mb_ctcp *self)
{
    int fd;

    fd = mb_net_connect (self->host, self->port, NULL);
    if (fd < 0)
        return fd;

    fcntl (fd, F_SETFL, fcntl (fd, F_GETFL, 0) | O_NONBLOCK);

    self->sipc = (struct mb_sipc *) mb_alloc (sizeof (struct mb_sipc));
    if (!self->sipc) {
        close (fd);
        return -ENOMEM;
    }

    mb_sipc_create (self->sipc, self->ep, fd);
    mb_sipc_start (self->sipc);
    return 0;
}

int mb_ctcp_create (struct mb_ep *ep)
{
    struct mb_ctcp *self;
    int rc;

    self = (struct mb_ctcp *) mb_alloc (sizeof (struct mb_ctcp));
    if (!self)
        return -ENOMEM;

    self->ep = ep;
    self->sipc = NULL;
    self->running = 1;
    mb_mutex_init (&self->lock);
    mb_thread_init (&self->reconnect_thread);

    rc = mb_net_parse_addr (mb_ep_getaddr (ep), self->host,
        sizeof (self->host), &self->port);
    if (rc < 0) {
        mb_free (self);
        return rc;
    }

    mb_ep_tran_setup (ep, &mb_ctcp_ops, self);

    rc = mb_ctcp_do_connect (self);
    if (rc >= 0)
        return 0;

    if (mb_ep_sock (ep)->reconnect_ivl > 0) {
        mb_thread_start (&self->reconnect_thread,
            mb_ctcp_reconnect_loop, self);
        return 0;
    }

    mb_mutex_term (&self->lock);
    mb_free (self);
    return rc;
}

static void mb_ctcp_stop (void *p)
{
    struct mb_ctcp *self = (struct mb_ctcp *) p;

    mb_mutex_lock (&self->lock);
    self->running = 0;
    if (self->sipc) {
        mb_sipc_stop (self->sipc);
        mb_sipc_term (self->sipc);
        mb_free (self->sipc);
        self->sipc = NULL;
    }
    mb_mutex_unlock (&self->lock);

    mb_thread_term (&self->reconnect_thread);
    mb_ep_stopped (self->ep);
}

static void mb_ctcp_destroy (void *p)
{
    struct mb_ctcp *self = (struct mb_ctcp *) p;

    if (self->sipc) {
        mb_sipc_stop (self->sipc);
        mb_sipc_term (self->sipc);
        mb_free (self->sipc);
    }

    mb_mutex_term (&self->lock);
    mb_free (self);
}
