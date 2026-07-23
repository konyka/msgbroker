#include "ctls.h"
#include "../../core/ep.h"
#include "../../core/sock.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/net.h"
#include "../../pal/sleep.h"

#include <msgbroker/mb.h>

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

static void mb_ctls_stop (void *p);
static void mb_ctls_destroy (void *p);
static void mb_ctls_on_disconnect (void *p);

static const struct mb_ep_ops mb_ctls_ops = {
    mb_ctls_stop,
    mb_ctls_destroy,
    mb_ctls_on_disconnect,
};

static int mb_ctls_ssl_wait (SSL *ssl, int want, volatile int *running,
    int *budget)
{
    int fd = SSL_get_fd (ssl);

    while (*budget > 0) {
        struct pollfd pfd;
        int slice = *budget > 50 ? 50 : *budget;
        int rc;

        if (running && !*running)
            return -ECANCELED;

        pfd.fd = fd;
        pfd.events = (want == SSL_ERROR_WANT_READ) ? POLLIN : POLLOUT;
        pfd.revents = 0;
        rc = poll (&pfd, 1, slice);
        if (rc < 0) {
            if (errno == EINTR)
                continue;
            return -errno;
        }
        if (rc == 0) {
            *budget -= slice;
            continue;
        }
        return 0;
    }
    return -ETIMEDOUT;
}

static SSL *mb_ctls_do_ssl_connect (struct mb_ctls *self, int fd,
    volatile int *running, int timeout_ms)
{
    struct mb_sock *sock;
    SSL_CTX *ctx;
    SSL *ssl;
    int budget;

    ctx = SSL_CTX_new (TLS_client_method ());
    if (!ctx)
        return NULL;

    SSL_CTX_set_min_proto_version (ctx, TLS1_2_VERSION);

    sock = mb_ep_sock (self->ep);
    if (!sock->tls_verify) {
        SSL_CTX_set_verify (ctx, SSL_VERIFY_NONE, NULL);
    } else {
        SSL_CTX_set_verify (ctx, SSL_VERIFY_PEER, NULL);
        if (sock->tls_ca_path[0])
            SSL_CTX_load_verify_locations (ctx, sock->tls_ca_path, NULL);
    }

    ssl = SSL_new (ctx);
    SSL_CTX_free (ctx);
    if (!ssl)
        return NULL;

    SSL_set_fd (ssl, fd);
    SSL_set_connect_state (ssl);

    budget = timeout_ms > 0 ? timeout_ms : 5000;
    for (;;) {
        int rc;
        int err;

        if (running && !*running) {
            SSL_free (ssl);
            return NULL;
        }

        rc = SSL_connect (ssl);
        if (rc == 1)
            return ssl;

        err = SSL_get_error (ssl, rc);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            if (mb_ctls_ssl_wait (ssl, err, running, &budget) < 0) {
                SSL_free (ssl);
                return NULL;
            }
            continue;
        }

        SSL_free (ssl);
        return NULL;
    }
}

static void mb_ctls_free_zombie (struct mb_ctls *self)
{
    if (self->zombie) {
        mb_stls_term (self->zombie);
        mb_free (self->zombie);
        self->zombie = NULL;
    }
}

static void mb_ctls_reconnect_loop (void *arg)
{
    struct mb_ctls *self = (struct mb_ctls *) arg;
    int ivl = mb_ep_sock (self->ep)->reconnect_ivl;
    int ivl_max = mb_ep_sock (self->ep)->reconnect_ivl_max;
    int current_ivl = mb_reconnect_cap_ivl (ivl, ivl_max);

    mb_mutex_lock (&self->lock);
    mb_ctls_free_zombie (self);
    mb_mutex_unlock (&self->lock);

    while (self->running) {
        int fd;
        SSL *ssl;
        struct mb_stls *stls;

        fd = mb_net_connect_cached (self->host, self->port, NULL,
            &self->running, 5000, &self->resolved,
            self->ep->options.ipv4only);
        if (fd < 0) {
            if (fd == -ECANCELED)
                break;
            mb_msleep_while (&self->running, current_ivl);
            current_ivl = mb_reconnect_next_ivl (current_ivl, ivl_max);
            continue;
        }

        ssl = mb_ctls_do_ssl_connect (self, fd, &self->running, 5000);
        if (!ssl) {
            close (fd);
            if (!self->running)
                break;
            mb_msleep_while (&self->running, current_ivl);
            current_ivl = mb_reconnect_next_ivl (current_ivl, ivl_max);
            continue;
        }

        stls = (struct mb_stls *) mb_alloc (sizeof (struct mb_stls));
        if (!stls) {
            SSL_free (ssl);
            close (fd);
            mb_msleep_while (&self->running, current_ivl);
            continue;
        }

        mb_stls_create (stls, self->ep, ssl);

        mb_mutex_lock (&self->lock);
        if (!self->running) {
            mb_stls_term (stls);
            mb_free (stls);
            self->reconnecting = 0;
            mb_mutex_unlock (&self->lock);
            return;
        }
        self->stls = stls;
        mb_stls_set_on_error (stls, mb_ctls_on_disconnect, self);
        {
            int rc = mb_stls_start (stls);
            if (rc < 0) {
                self->stls = NULL;
                mb_stls_term (stls);
                mb_free (stls);
                mb_mutex_unlock (&self->lock);
                /* Permanent protocol reject — do not spin forever. */
                if (rc == -EISCONN)
                    break;
                mb_msleep_while (&self->running, current_ivl);
                continue;
            }
        }
        self->reconnecting = 0;
        mb_mutex_unlock (&self->lock);
        return;
    }

    mb_mutex_lock (&self->lock);
    self->reconnecting = 0;
    mb_mutex_unlock (&self->lock);
}

static int mb_ctls_do_connect (struct mb_ctls *self)
{
    int fd;
    SSL *ssl;

    fd = mb_net_connect_cached (self->host, self->port, NULL,
        &self->running, 5000, &self->resolved,
        self->ep->options.ipv4only);
    if (fd < 0)
        return fd;

    ssl = mb_ctls_do_ssl_connect (self, fd, &self->running, 5000);
    if (!ssl) {
        close (fd);
        return self->running ? -ECONNREFUSED : -ECANCELED;
    }

    self->stls = (struct mb_stls *) mb_alloc (sizeof (struct mb_stls));
    if (!self->stls) {
        SSL_free (ssl);
        close (fd);
        return -ENOMEM;
    }

    mb_stls_create (self->stls, self->ep, ssl);
    mb_stls_set_on_error (self->stls, mb_ctls_on_disconnect, self);
    {
        int rc = mb_stls_start (self->stls);
        if (rc < 0) {
            mb_stls_term (self->stls);
            mb_free (self->stls);
            self->stls = NULL;
            return rc;
        }
    }
    return 0;
}

int mb_ctls_create (struct mb_ep *ep)
{
    struct mb_ctls *self;
    int rc;

    self = (struct mb_ctls *) mb_alloc (sizeof (struct mb_ctls));
    if (!self)
        return -ENOMEM;

    self->ep = ep;
    self->stls = NULL;
    self->zombie = NULL;
    self->running = 1;
    self->reconnecting = 0;
    self->resolved.ready = 0;
    mb_mutex_init (&self->lock);
    mb_thread_init (&self->reconnect_thread);

    rc = mb_net_parse_addr (mb_ep_getaddr (ep), self->host,
        sizeof (self->host), &self->port);
    if (rc < 0) {
        mb_free (self);
        return rc;
    }

    mb_ep_tran_setup (ep, &mb_ctls_ops, self);

    rc = mb_ctls_do_connect (self);
    if (rc >= 0)
        return 0;

    if (mb_ep_sock (ep)->reconnect_ivl > 0 && rc != -EISCONN) {
        int tries;
        int started = 0;

        self->reconnecting = 1;
        for (tries = 0; tries < 5; tries++) {
            if (mb_thread_start (&self->reconnect_thread,
                    mb_ctls_reconnect_loop, self) == 0) {
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

static void mb_ctls_on_disconnect (void *p)
{
    struct mb_ctls *self = (struct mb_ctls *) p;
    int start_reconnect = 0;

    mb_mutex_lock (&self->lock);
    if (!self->running) {
        mb_mutex_unlock (&self->lock);
        return;
    }

    if (self->stls) {
        mb_stls_stop (self->stls);
        mb_ctls_free_zombie (self);
        self->zombie = self->stls;
        self->stls = NULL;
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
                        mb_ctls_reconnect_loop, self) == 0) {
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

static void mb_ctls_stop (void *p)
{
    struct mb_ctls *self = (struct mb_ctls *) p;

    mb_mutex_lock (&self->lock);
    self->running = 0;
    if (self->stls) {
        mb_stls_stop (self->stls);
        mb_stls_term (self->stls);
        mb_free (self->stls);
        self->stls = NULL;
    }
    mb_ctls_free_zombie (self);
    mb_mutex_unlock (&self->lock);

    mb_thread_term (&self->reconnect_thread);
    mb_ep_stopped (self->ep);
}

static void mb_ctls_destroy (void *p)
{
    struct mb_ctls *self = (struct mb_ctls *) p;

    if (self->stls) {
        mb_stls_stop (self->stls);
        mb_stls_term (self->stls);
        mb_free (self->stls);
    }
    mb_ctls_free_zombie (self);

    mb_mutex_term (&self->lock);
    mb_free (self);
}
