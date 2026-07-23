#include "cipc.h"
#include "sipc.h"
#include "../../core/ep.h"
#include "../../core/sock.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/net.h"
#include "../../pal/sleep.h"

#include <msgbroker/mb.h>

#include <string.h>
#include <errno.h>
#include <unistd.h>

static void mb_cipc_stop (void *p);
static void mb_cipc_destroy (void *p);
static void mb_cipc_on_disconnect (void *p);

static const struct mb_ep_ops mb_cipc_ops = {
    mb_cipc_stop,
    mb_cipc_destroy,
    mb_cipc_on_disconnect,
};

static int mb_cipc_parse_addr (const char *addr, char *path, size_t pathlen)
{
    const char *sep;
    size_t len;

    sep = strstr (addr, "://");
    if (!sep)
        return -EINVAL;
    sep += 3;
    len = strlen (sep);
    /* Must fit with NUL inside sun_path-sized buffer; do not truncate. */
    if (len >= pathlen)
        return -ENAMETOOLONG;
    memcpy (path, sep, len + 1);
    return 0;
}

static void mb_cipc_free_zombie (struct mb_cipc *self)
{
    if (self->zombie) {
        mb_sipc_term (self->zombie);
        mb_free (self->zombie);
        self->zombie = NULL;
    }
}

static void mb_cipc_reconnect_loop (void *arg)
{
    struct mb_cipc *self = (struct mb_cipc *) arg;
    int ivl = mb_ep_sock (self->ep)->reconnect_ivl;
    int ivl_max = mb_ep_sock (self->ep)->reconnect_ivl_max;
    int current_ivl = ivl;

    mb_mutex_lock (&self->lock);
    mb_cipc_free_zombie (self);
    mb_mutex_unlock (&self->lock);

    while (self->running) {
        int fd;
        struct mb_sipc *sipc;

        fd = mb_net_unix_connect_while (self->path, &self->running, 5000);
        if (fd < 0) {
            if (fd == -ECANCELED)
                break;
            mb_msleep_while (&self->running, current_ivl);
            current_ivl = mb_reconnect_next_ivl (current_ivl, ivl_max);
            continue;
        }

        sipc = (struct mb_sipc *) mb_alloc (sizeof (struct mb_sipc));
        if (!sipc) {
            close (fd);
            mb_msleep_while (&self->running, current_ivl);
            continue;
        }

        mb_sipc_create (sipc, self->ep, fd);

        mb_mutex_lock (&self->lock);
        if (!self->running) {
            mb_sipc_term (sipc);
            mb_free (sipc);
            self->reconnecting = 0;
            mb_mutex_unlock (&self->lock);
            return;
        }
        self->sipc = sipc;
        mb_sipc_set_on_error (sipc, mb_cipc_on_disconnect, self);
        if (mb_sipc_start (sipc) < 0) {
            self->sipc = NULL;
            mb_sipc_term (sipc);
            mb_free (sipc);
            mb_mutex_unlock (&self->lock);
            mb_msleep_while (&self->running, current_ivl);
            continue;
        }
        self->reconnecting = 0;
        mb_mutex_unlock (&self->lock);
        return;
    }

    mb_mutex_lock (&self->lock);
    self->reconnecting = 0;
    mb_mutex_unlock (&self->lock);
}

static int mb_cipc_do_connect (struct mb_cipc *self)
{
    int fd;

    fd = mb_net_unix_connect_while (self->path, &self->running, 5000);
    if (fd < 0)
        return fd;

    self->sipc = (struct mb_sipc *) mb_alloc (sizeof (struct mb_sipc));
    if (!self->sipc) {
        close (fd);
        return -ENOMEM;
    }

    mb_sipc_create (self->sipc, self->ep, fd);
    mb_sipc_set_on_error (self->sipc, mb_cipc_on_disconnect, self);
    {
        int rc = mb_sipc_start (self->sipc);
        if (rc < 0) {
            mb_sipc_term (self->sipc);
            mb_free (self->sipc);
            self->sipc = NULL;
            return rc;
        }
    }
    return 0;
}

int mb_cipc_create (struct mb_ep *ep)
{
    struct mb_cipc *self;
    int rc;

    self = (struct mb_cipc *) mb_alloc (sizeof (struct mb_cipc));
    if (!self)
        return -ENOMEM;

    self->ep = ep;
    self->sipc = NULL;
    self->zombie = NULL;
    self->running = 1;
    self->reconnecting = 0;
    memset (self->path, 0, sizeof (self->path));
    rc = mb_cipc_parse_addr (mb_ep_getaddr (ep), self->path,
        sizeof (self->path));
    if (rc < 0) {
        mb_free (self);
        return rc;
    }

    mb_mutex_init (&self->lock);
    mb_thread_init (&self->reconnect_thread);

    mb_ep_tran_setup (ep, &mb_cipc_ops, self);

    rc = mb_cipc_do_connect (self);
    if (rc >= 0)
        return 0;

    if (mb_ep_sock (ep)->reconnect_ivl > 0 && rc != -EISCONN) {
        int tries;
        int started = 0;

        self->reconnecting = 1;
        for (tries = 0; tries < 5; tries++) {
            if (mb_thread_start (&self->reconnect_thread,
                    mb_cipc_reconnect_loop, self) == 0) {
                started = 1;
                break;
            }
            mb_msleep (1 << tries);
            mb_thread_init (&self->reconnect_thread);
        }
        if (!started) {
            self->reconnecting = 0;
            mb_mutex_term (&self->lock);
            mb_free (self);
            return -EAGAIN;
        }
        return 0;
    }

    mb_mutex_term (&self->lock);
    mb_free (self);
    return rc;
}

static void mb_cipc_on_disconnect (void *p)
{
    struct mb_cipc *self = (struct mb_cipc *) p;
    int start_reconnect = 0;

    mb_mutex_lock (&self->lock);
    if (!self->running) {
        mb_mutex_unlock (&self->lock);
        return;
    }

    if (self->sipc) {
        mb_sipc_stop (self->sipc);
        mb_cipc_free_zombie (self);
        self->zombie = self->sipc;
        self->sipc = NULL;
    }

    if (mb_ep_sock (self->ep)->reconnect_ivl > 0 && !self->reconnecting) {
        self->reconnecting = 1;
        start_reconnect = 1;
    }
    mb_mutex_unlock (&self->lock);

    if (start_reconnect) {
        mb_thread_term (&self->reconnect_thread);
        mb_thread_init (&self->reconnect_thread);
        {
            int tries;
            int started = 0;
            for (tries = 0; tries < 5; tries++) {
                if (mb_thread_start (&self->reconnect_thread,
                        mb_cipc_reconnect_loop, self) == 0) {
                    started = 1;
                    break;
                }
                mb_msleep (1 << tries);
                mb_thread_init (&self->reconnect_thread);
            }
            if (!started) {
                mb_mutex_lock (&self->lock);
                self->reconnecting = 0;
                mb_mutex_unlock (&self->lock);
            }
        }
    }
}

static void mb_cipc_stop (void *p)
{
    struct mb_cipc *self = (struct mb_cipc *) p;

    mb_mutex_lock (&self->lock);
    self->running = 0;
    if (self->sipc) {
        mb_sipc_stop (self->sipc);
        mb_sipc_term (self->sipc);
        mb_free (self->sipc);
        self->sipc = NULL;
    }
    mb_cipc_free_zombie (self);
    mb_mutex_unlock (&self->lock);

    mb_thread_term (&self->reconnect_thread);
    mb_ep_stopped (self->ep);
}

static void mb_cipc_destroy (void *p)
{
    struct mb_cipc *self = (struct mb_cipc *) p;

    if (self->sipc) {
        mb_sipc_stop (self->sipc);
        mb_sipc_term (self->sipc);
        mb_free (self->sipc);
    }
    mb_cipc_free_zombie (self);

    mb_mutex_term (&self->lock);
    mb_free (self);
}
