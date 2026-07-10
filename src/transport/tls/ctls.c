#include "ctls.h"
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

static SSL *mb_ctls_do_ssl_connect (struct mb_ctls *self, int fd)
{
    struct mb_sock *sock;
    SSL_CTX *ctx;
    SSL *ssl;

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

    if (SSL_connect (ssl) <= 0) {
        SSL_free (ssl);
        return NULL;
    }

    return ssl;
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
    int current_ivl = ivl;

    mb_mutex_lock (&self->lock);
    mb_ctls_free_zombie (self);
    mb_mutex_unlock (&self->lock);

    while (self->running) {
        int fd;
        SSL *ssl;
        struct mb_stls *stls;

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

        ssl = mb_ctls_do_ssl_connect (self, fd);
        if (!ssl) {
            close (fd);
            struct pollfd pfd = { .fd = -1, .events = 0 };
            poll (&pfd, 0, current_ivl);
            if (ivl_max > 0 && current_ivl < ivl_max)
                current_ivl *= 2;
            if (current_ivl > ivl_max && ivl_max > 0)
                current_ivl = ivl_max;
            continue;
        }

        stls = (struct mb_stls *) mb_alloc (sizeof (struct mb_stls));
        if (!stls) {
            SSL_free (ssl);
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
        mb_stls_start (stls);
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

    fd = mb_net_connect (self->host, self->port, NULL);
    if (fd < 0)
        return fd;

    ssl = mb_ctls_do_ssl_connect (self, fd);
    if (!ssl) {
        close (fd);
        return -ECONNREFUSED;
    }

    self->stls = (struct mb_stls *) mb_alloc (sizeof (struct mb_stls));
    if (!self->stls) {
        SSL_free (ssl);
        return -ENOMEM;
    }

    mb_stls_create (self->stls, self->ep, ssl);
    mb_stls_set_on_error (self->stls, mb_ctls_on_disconnect, self);
    mb_stls_start (self->stls);
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

    if (mb_ep_sock (ep)->reconnect_ivl > 0) {
        self->reconnecting = 1;
        mb_thread_start (&self->reconnect_thread,
            mb_ctls_reconnect_loop, self);
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
        mb_thread_start (&self->reconnect_thread,
            mb_ctls_reconnect_loop, self);
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
