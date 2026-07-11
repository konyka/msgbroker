#include "btls.h"
#include "stls.h"
#include "../../core/ep.h"
#include "../../core/sock.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/net.h"
#include "../../utils/list.h"
#include "../../utils/cont.h"

#include <msgbroker/mb.h>

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define MB_BTLS_BACKLOG 10

static void mb_btls_stop (void *p);
static void mb_btls_destroy (void *p);
static void mb_btls_on_session_error (void *p);
static void mb_btls_free_zombies (struct mb_btls *self);

static const struct mb_ep_ops mb_btls_ops = {
    mb_btls_stop,
    mb_btls_destroy,
    NULL,
};

static void mb_btls_free_zombies (struct mb_btls *self)
{
    while (!mb_list_empty (&self->zombies)) {
        struct mb_list_item *it = mb_list_begin (&self->zombies);
        struct mb_stls *stls = mb_cont (it, struct mb_stls, item);
        mb_list_erase (&self->zombies, it);
        mb_stls_term (stls);
        mb_free (stls);
    }
}

static void mb_btls_on_session_error (void *p)
{
    struct mb_btls *self = (struct mb_btls *) p;
    struct mb_list_item *it;
    struct mb_list_item *next;

    mb_mutex_lock (&self->lock);
    for (it = mb_list_begin (&self->stlss); it != mb_list_end (&self->stlss);
        it = next) {
        struct mb_stls *stls = mb_cont (it, struct mb_stls, item);
        next = mb_list_next (&self->stlss, it);
        if (!stls->disconnected)
            continue;
        mb_list_erase (&self->stlss, it);
        mb_stls_stop (stls);
        mb_list_insert (&self->zombies, &stls->item,
            mb_list_end (&self->zombies));
    }
    mb_mutex_unlock (&self->lock);
}

static void mb_btls_accept_loop (void *arg)
{
    struct mb_btls *self = (struct mb_btls *) arg;

    while (self->running) {
        struct pollfd pfd;
        int rc;

        mb_mutex_lock (&self->lock);
        mb_btls_free_zombies (self);
        mb_mutex_unlock (&self->lock);

        pfd.fd = self->listen_fd;
        pfd.events = POLLIN;
        rc = poll (&pfd, 1, 100);

        if (rc <= 0)
            continue;

        if (pfd.revents & POLLIN) {
            struct sockaddr_storage client;
            socklen_t client_len = sizeof (client);
            int client_fd;
            SSL *ssl;
            struct mb_stls *stls;
            int flag = 1;

            client_fd = accept (self->listen_fd,
                (struct sockaddr *) &client, &client_len);
            if (client_fd < 0)
                continue;

            setsockopt (client_fd, IPPROTO_TCP, TCP_NODELAY,
                &flag, sizeof (flag));

            ssl = SSL_new (self->ctx);
            if (!ssl) {
                close (client_fd);
                continue;
            }

            SSL_set_fd (ssl, client_fd);
            SSL_set_accept_state (ssl);

            if (SSL_accept (ssl) <= 0) {
                SSL_free (ssl);
                close (client_fd);
                continue;
            }

            fcntl (client_fd, F_SETFL,
                fcntl (client_fd, F_GETFL, 0) | O_NONBLOCK);

            stls = (struct mb_stls *) mb_alloc (sizeof (struct mb_stls));
            if (!stls) {
                SSL_free (ssl);
                close (client_fd);
                continue;
            }

            mb_stls_create (stls, self->ep, ssl);
            mb_stls_set_on_error (stls, mb_btls_on_session_error, self);

            mb_mutex_lock (&self->lock);
            mb_stls_start (stls);
            mb_list_insert (&self->stlss, &stls->item,
                mb_list_end (&self->stlss));
            mb_mutex_unlock (&self->lock);
        }
    }
}

int mb_btls_create (struct mb_ep *ep)
{
    struct mb_btls *self;
    int fd;
    int rc;
    char host[256];
    uint16_t port;

    rc = mb_net_parse_addr (mb_ep_getaddr (ep), host, sizeof (host), &port);
    if (rc < 0)
        return rc;

    fd = mb_net_bind (host, port, MB_BTLS_BACKLOG);
    if (fd < 0)
        return fd;

    self = (struct mb_btls *) mb_alloc (sizeof (struct mb_btls));
    if (!self) {
        close (fd);
        return -ENOMEM;
    }

    self->ep = ep;
    self->listen_fd = fd;
    self->ctx = SSL_CTX_new (TLS_server_method ());
    if (!self->ctx) {
        close (fd);
        mb_free (self);
        return -EINVAL;
    }

    SSL_CTX_set_min_proto_version (self->ctx, TLS1_2_VERSION);

    {
        struct mb_sock *sock = mb_ep_sock (ep);
        if (sock->tls_cert_path[0]) {
            if (SSL_CTX_use_certificate_file (self->ctx,
                    sock->tls_cert_path, SSL_FILETYPE_PEM) <= 0) {
                SSL_CTX_free (self->ctx);
                close (fd);
                mb_free (self);
                return -EINVAL;
            }
        }
        if (sock->tls_key_path[0]) {
            if (SSL_CTX_use_PrivateKey_file (self->ctx,
                    sock->tls_key_path, SSL_FILETYPE_PEM) <= 0) {
                SSL_CTX_free (self->ctx);
                close (fd);
                mb_free (self);
                return -EINVAL;
            }
        }
        if (sock->tls_ca_path[0]) {
            SSL_CTX_load_verify_locations (self->ctx,
                sock->tls_ca_path, NULL);
        }
    }

    mb_list_init (&self->stlss);
    mb_list_init (&self->zombies);
    mb_mutex_init (&self->lock);
    self->running = 1;

    mb_ep_tran_setup (ep, &mb_btls_ops, self);

    mb_thread_init (&self->accept_thread);
    mb_thread_start (&self->accept_thread, mb_btls_accept_loop, self);

    return 0;
}

static void mb_btls_cleanup (struct mb_btls *self)
{
    while (!mb_list_empty (&self->stlss)) {
        struct mb_list_item *it = mb_list_begin (&self->stlss);
        struct mb_stls *stls = mb_cont (it, struct mb_stls, item);
        mb_list_erase (&self->stlss, it);
        mb_stls_stop (stls);
        mb_stls_term (stls);
        mb_free (stls);
    }
    mb_btls_free_zombies (self);

    if (self->listen_fd >= 0) {
        close (self->listen_fd);
        self->listen_fd = -1;
    }

    if (self->ctx) {
        SSL_CTX_free (self->ctx);
        self->ctx = NULL;
    }
}

static void mb_btls_stop (void *p)
{
    struct mb_btls *self = (struct mb_btls *) p;

    self->running = 0;
    mb_thread_term (&self->accept_thread);

    mb_mutex_lock (&self->lock);
    mb_btls_cleanup (self);
    mb_mutex_unlock (&self->lock);

    mb_ep_stopped (self->ep);
}

static void mb_btls_destroy (void *p)
{
    struct mb_btls *self = (struct mb_btls *) p;

    if (self->running)
        mb_btls_stop (p);

    mb_mutex_term (&self->lock);
    mb_list_term (&self->stlss);
    mb_list_term (&self->zombies);
    mb_free (self);
}
