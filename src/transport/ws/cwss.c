#include "bwss.h"
#include "sws.h"
#include "ws.h"
#include "../../core/ep.h"
#include "../../core/sock.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/net.h"
#include "../../pal/thread.h"
#include "../../pal/mutex.h"
#include "../../pal/sleep.h"

#include <msgbroker/mb.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

static void mb_cwss_stop (void *p);
static void mb_cwss_destroy (void *p);
static void mb_cwss_on_disconnect (void *p);

static const struct mb_ep_ops mb_cwss_ops = {
    mb_cwss_stop,
    mb_cwss_destroy,
    mb_cwss_on_disconnect,
};

struct mb_cwss {
    struct mb_ep *ep;
    struct mb_sws *sws;
    struct mb_sws *zombie;   /* stopped session awaiting free */
    int running;
    int reconnecting;
    struct mb_thread reconnect_thread;
    struct mb_mutex lock;
    char host[256];
    uint16_t port;
};

static size_t mb_cwss_b64_encode (const uint8_t *src, size_t len, char *dst)
{
    static const char b64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i, j = 0;
    for (i = 0; i < len; i += 3) {
        uint32_t v = (uint32_t) src[i] << 16;
        if (i + 1 < len) v |= (uint32_t) src[i+1] << 8;
        if (i + 2 < len) v |= src[i+2];
        dst[j++] = b64[(v >> 18) & 0x3F];
        dst[j++] = b64[(v >> 12) & 0x3F];
        dst[j++] = (i + 1 < len) ? b64[(v >> 6) & 0x3F] : '=';
        dst[j++] = (i + 2 < len) ? b64[v & 0x3F] : '=';
    }
    dst[j] = '\0';
    return j;
}

static int mb_cwss_ssl_wait (SSL *ssl, int want, volatile int *running,
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

static int mb_cwss_ssl_write_all (SSL *ssl, const void *data, size_t len,
    volatile int *running, int *budget)
{
    const uint8_t *ptr = (const uint8_t *) data;
    size_t sent = 0;

    while (sent < len) {
        int ns;

        if (running && !*running)
            return -ECANCELED;

        ns = SSL_write (ssl, ptr + sent, (int) (len - sent));
        if (ns > 0) {
            sent += (size_t) ns;
            continue;
        }
        {
            int err = SSL_get_error (ssl, ns);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                if (mb_cwss_ssl_wait (ssl, err, running, budget) < 0)
                    return -1;
                continue;
            }
            return -1;
        }
    }
    return 0;
}

static int mb_cwss_ssl_read_http (SSL *ssl, char *buf, size_t buflen,
    volatile int *running, int *budget)
{
    size_t pos = 0;

    while (pos < buflen - 1) {
        int nr;

        if (running && !*running)
            return -ECANCELED;

        nr = SSL_read (ssl, buf + pos, 1);
        if (nr > 0) {
            pos += (size_t) nr;
            buf[pos] = '\0';
            if (pos >= 4 && memcmp (buf + pos - 4, "\r\n\r\n", 4) == 0)
                break;
            continue;
        }
        {
            int err = SSL_get_error (ssl, nr);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                if (mb_cwss_ssl_wait (ssl, err, running, budget) < 0)
                    return -1;
                continue;
            }
            return -1;
        }
    }
    return 0;
}

static int mb_cwss_do_handshake (SSL *ssl, const char *host, uint16_t port,
    volatile int *running, int timeout_ms)
{
    uint8_t key_bytes[16];
    char key_b64[32];
    char req[512];
    char resp[4096];
    int i;
    int budget = timeout_ms > 0 ? timeout_ms : 5000;

    srand ((unsigned int) time (NULL) ^ (unsigned int) SSL_get_fd (ssl));
    for (i = 0; i < 16; ++i)
        key_bytes[i] = (uint8_t) (rand () & 0xFF);
    mb_cwss_b64_encode (key_bytes, 16, key_b64);

    snprintf (req, sizeof (req),
        "GET / HTTP/1.1\r\n"
        "Host: %s:%u\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n", host, port, key_b64);

    if (mb_cwss_ssl_write_all (ssl, req, strlen (req), running, &budget) < 0)
        return -1;

    if (mb_cwss_ssl_read_http (ssl, resp, sizeof (resp), running, &budget) < 0)
        return -1;

    if (memcmp (resp, "HTTP/1.1 101", 12) != 0 &&
        memcmp (resp, "HTTP/1.0 101", 12) != 0)
        return -1;

    return 0;
}

static SSL *mb_cwss_do_tls_connect (struct mb_cwss *self, int fd,
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
            if (mb_cwss_ssl_wait (ssl, err, running, &budget) < 0) {
                SSL_free (ssl);
                return NULL;
            }
            continue;
        }

        SSL_free (ssl);
        return NULL;
    }
}

static void mb_cwss_free_zombie (struct mb_cwss *self)
{
    if (self->zombie) {
        mb_sws_term (self->zombie);
        mb_free (self->zombie);
        self->zombie = NULL;
    }
}

static void mb_cwss_reconnect_loop (void *arg)
{
    struct mb_cwss *self = (struct mb_cwss *) arg;
    int ivl = mb_ep_sock (self->ep)->reconnect_ivl;
    int ivl_max = mb_ep_sock (self->ep)->reconnect_ivl_max;
    int current_ivl = ivl;

    mb_mutex_lock (&self->lock);
    mb_cwss_free_zombie (self);
    mb_mutex_unlock (&self->lock);

    while (self->running) {
        int fd;
        SSL *ssl;
        struct mb_sws *sws;

        fd = mb_net_connect_while (self->host, self->port, NULL,
            &self->running, 5000);
        if (fd < 0) {
            if (fd == -ECANCELED)
                break;
            mb_msleep_while (&self->running, current_ivl);
            if (ivl_max > 0 && current_ivl < ivl_max)
                current_ivl *= 2;
            if (current_ivl > ivl_max && ivl_max > 0)
                current_ivl = ivl_max;
            continue;
        }

        ssl = mb_cwss_do_tls_connect (self, fd, &self->running, 5000);
        if (!ssl) {
            close (fd);
            if (!self->running)
                break;
            mb_msleep_while (&self->running, current_ivl);
            if (ivl_max > 0 && current_ivl < ivl_max)
                current_ivl *= 2;
            if (current_ivl > ivl_max && ivl_max > 0)
                current_ivl = ivl_max;
            continue;
        }

        if (mb_cwss_do_handshake (ssl, self->host, self->port,
                &self->running, 5000) < 0) {
            SSL_free (ssl);
            close (fd);
            if (!self->running)
                break;
            mb_msleep_while (&self->running, current_ivl);
            if (ivl_max > 0 && current_ivl < ivl_max)
                current_ivl *= 2;
            if (current_ivl > ivl_max && ivl_max > 0)
                current_ivl = ivl_max;
            continue;
        }

        sws = (struct mb_sws *) mb_alloc (sizeof (struct mb_sws));
        if (!sws) {
            SSL_free (ssl);
            close (fd);
            mb_msleep_while (&self->running, current_ivl);
            continue;
        }

        mb_sws_create (sws, self->ep, fd, 1);
        sws->ssl = ssl;
        mb_sws_set_on_error (sws, mb_cwss_on_disconnect, self);

        mb_mutex_lock (&self->lock);
        if (!self->running) {
            mb_sws_term (sws);
            mb_free (sws);
            self->reconnecting = 0;
            mb_mutex_unlock (&self->lock);
            return;
        }
        self->sws = sws;
        if (mb_sws_start (sws) < 0) {
            self->sws = NULL;
            mb_sws_term (sws);
            mb_free (sws);
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

static int mb_cwss_do_connect (struct mb_cwss *self)
{
    int fd;
    SSL *ssl;
    int rc;

    fd = mb_net_connect_while (self->host, self->port, NULL,
        &self->running, 5000);
    if (fd < 0)
        return fd;

    ssl = mb_cwss_do_tls_connect (self, fd, &self->running, 5000);
    if (!ssl) {
        close (fd);
        return self->running ? -ECONNREFUSED : -ECANCELED;
    }

    rc = mb_cwss_do_handshake (ssl, self->host, self->port,
        &self->running, 5000);
    if (rc < 0) {
        SSL_free (ssl);
        close (fd);
        return self->running ? -ECONNREFUSED : -ECANCELED;
    }

    self->sws = (struct mb_sws *) mb_alloc (sizeof (struct mb_sws));
    if (!self->sws) {
        SSL_free (ssl);
        close (fd);
        return -ENOMEM;
    }

    mb_sws_create (self->sws, self->ep, fd, 1);
    self->sws->ssl = ssl;
    mb_sws_set_on_error (self->sws, mb_cwss_on_disconnect, self);
    if (mb_sws_start (self->sws) < 0) {
        mb_sws_term (self->sws);
        mb_free (self->sws);
        self->sws = NULL;
        return -ECONNREFUSED;
    }
    return 0;
}

int mb_cwss_create (struct mb_ep *ep)
{
    struct mb_cwss *self;
    int rc;

    self = (struct mb_cwss *) mb_alloc (sizeof (struct mb_cwss));
    if (!self)
        return -ENOMEM;

    self->ep = ep;
    self->sws = NULL;
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

    mb_ep_tran_setup (ep, &mb_cwss_ops, self);

    rc = mb_cwss_do_connect (self);
    if (rc >= 0)
        return 0;

    if (mb_ep_sock (ep)->reconnect_ivl > 0) {
        int tries;
        int started = 0;

        self->reconnecting = 1;
        for (tries = 0; tries < 5; tries++) {
            if (mb_thread_start (&self->reconnect_thread,
                    mb_cwss_reconnect_loop, self) == 0) {
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

static void mb_cwss_on_disconnect (void *p)
{
    struct mb_cwss *self = (struct mb_cwss *) p;
    int start_reconnect = 0;

    mb_mutex_lock (&self->lock);
    if (!self->running) {
        mb_mutex_unlock (&self->lock);
        return;
    }

    if (self->sws) {
        mb_sws_stop (self->sws);
        mb_cwss_free_zombie (self);
        self->zombie = self->sws;
        self->sws = NULL;
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
                        mb_cwss_reconnect_loop, self) == 0) {
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

static void mb_cwss_stop (void *p)
{
    struct mb_cwss *self = (struct mb_cwss *) p;

    mb_mutex_lock (&self->lock);
    self->running = 0;
    if (self->sws) {
        mb_sws_stop (self->sws);
        mb_sws_term (self->sws);
        mb_free (self->sws);
        self->sws = NULL;
    }
    mb_cwss_free_zombie (self);
    mb_mutex_unlock (&self->lock);

    mb_thread_term (&self->reconnect_thread);
    mb_ep_stopped (self->ep);
}

static void mb_cwss_destroy (void *p)
{
    struct mb_cwss *self = (struct mb_cwss *) p;

    if (self->sws) {
        mb_sws_stop (self->sws);
        mb_sws_term (self->sws);
        mb_free (self->sws);
    }
    mb_cwss_free_zombie (self);

    mb_mutex_term (&self->lock);
    mb_free (self);
}
