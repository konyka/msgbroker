#include "bwss.h"
#include "sws.h"
#include "ws.h"
#include "../../core/ep.h"
#include "../../core/sock.h"
#include "../../utils/alloc.h"
#include "../../utils/err.h"
#include "../../utils/net.h"
#include "../../utils/list.h"
#include "../../utils/cont.h"
#include "../../pal/thread.h"
#include "../../pal/mutex.h"

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
#include <openssl/ssl.h>
#include <openssl/err.h>

struct mb_bwss {
    struct mb_ep *ep;
    int listen_fd;
    SSL_CTX *ctx;
    struct mb_list sws_list;
    struct mb_list zombies;
    struct mb_mutex lock;
    volatile int running;
    struct mb_thread accept_thread;
};

static void mb_bwss_stop (void *p);
static void mb_bwss_destroy (void *p);
static void mb_bwss_on_session_error (void *p);
static void mb_bwss_free_zombies (struct mb_bwss *self);

static const struct mb_ep_ops mb_bwss_ops = {
    mb_bwss_stop,
    mb_bwss_destroy,
    NULL,
};

static void mb_bwss_free_zombies (struct mb_bwss *self)
{
    while (!mb_list_empty (&self->zombies)) {
        struct mb_list_item *it = mb_list_begin (&self->zombies);
        struct mb_sws *sws = mb_cont (it, struct mb_sws, item);
        mb_list_erase (&self->zombies, it);
        mb_sws_term (sws);
        mb_free (sws);
    }
}

static void mb_bwss_on_session_error (void *p)
{
    struct mb_bwss *self = (struct mb_bwss *) p;
    struct mb_list_item *it;
    struct mb_list_item *next;

    mb_mutex_lock (&self->lock);
    for (it = mb_list_begin (&self->sws_list);
        it != mb_list_end (&self->sws_list); it = next) {
        struct mb_sws *sws = mb_cont (it, struct mb_sws, item);
        next = mb_list_next (&self->sws_list, it);
        if (!sws->disconnected)
            continue;
        mb_list_erase (&self->sws_list, it);
        mb_sws_stop (sws);
        mb_list_insert (&self->zombies, &sws->item,
            mb_list_end (&self->zombies));
    }
    mb_mutex_unlock (&self->lock);
}

static const char mb_wss_accept_key[] =
    "258EAFA5-E914-47DA-95CA-5AB5F8A5B5E3";

static void mb_sha1 (const uint8_t *data, size_t len, uint8_t out[20])
{
    uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476, h4 = 0xC3D2E1F0;
    uint32_t w[80];
    size_t i;
    uint64_t bitlen = (uint64_t) len * 8;
    size_t padded_len = ((len + 8) / 64 + 1) * 64;
    uint8_t *msg = (uint8_t *) mb_alloc (padded_len);
    if (!msg) return;

    memcpy (msg, data, len);
    msg[len] = 0x80;
    memset (msg + len + 1, 0, padded_len - len - 9);
    msg[padded_len - 8] = (uint8_t) (bitlen >> 56);
    msg[padded_len - 7] = (uint8_t) (bitlen >> 48);
    msg[padded_len - 6] = (uint8_t) (bitlen >> 40);
    msg[padded_len - 5] = (uint8_t) (bitlen >> 32);
    msg[padded_len - 4] = (uint8_t) (bitlen >> 24);
    msg[padded_len - 3] = (uint8_t) (bitlen >> 16);
    msg[padded_len - 2] = (uint8_t) (bitlen >> 8);
    msg[padded_len - 1] = (uint8_t) bitlen;

    for (i = 0; i < padded_len; i += 64) {
        int j;
        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (j = 0; j < 16; ++j)
            w[j] = ((uint32_t) msg[i + j * 4] << 24) |
                   ((uint32_t) msg[i + j * 4 + 1] << 16) |
                   ((uint32_t) msg[i + j * 4 + 2] << 8) |
                   msg[i + j * 4 + 3];
        for (j = 16; j < 80; ++j) {
            uint32_t tmp = w[j-3] ^ w[j-8] ^ w[j-14] ^ w[j-16];
            w[j] = (tmp << 1) | (tmp >> 31);
        }
        for (j = 0; j < 80; ++j) {
            uint32_t f, k, tmp;
            if (j < 20) {
                f = (b & c) | ((~b) & d); k = 0x5A827999;
            } else if (j < 40) {
                f = b ^ c ^ d; k = 0x6ED9EBA1;
            } else if (j < 60) {
                f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d; k = 0xCA62C1D6;
            }
            tmp = ((a << 5) | (a >> 27)) + f + e + k + w[j];
            e = d; d = c; c = (b << 30) | (b >> 2); b = a; a = tmp;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }
    mb_free (msg);

    {
        uint8_t *o = out;
        o[0] = (uint8_t)(h0>>24); o[1] = (uint8_t)(h0>>16);
        o[2] = (uint8_t)(h0>>8); o[3] = (uint8_t)h0;
        o[4] = (uint8_t)(h1>>24); o[5] = (uint8_t)(h1>>16);
        o[6] = (uint8_t)(h1>>8); o[7] = (uint8_t)h1;
        o[8] = (uint8_t)(h2>>24); o[9] = (uint8_t)(h2>>16);
        o[10] = (uint8_t)(h2>>8); o[11] = (uint8_t)h2;
        o[12] = (uint8_t)(h3>>24); o[13] = (uint8_t)(h3>>16);
        o[14] = (uint8_t)(h3>>8); o[15] = (uint8_t)h3;
        o[16] = (uint8_t)(h4>>24); o[17] = (uint8_t)(h4>>16);
        o[18] = (uint8_t)(h4>>8); o[19] = (uint8_t)h4;
    }
}

static size_t mb_b64_encode (const uint8_t *src, size_t len, char *dst)
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

static char *mb_wss_find_header (const char *req, const char *name)
{
    const char *p = strstr (req, name);
    if (!p) return NULL;
    p += strlen (name);
    while (*p == ' ') p++;
    return (char *) p;
}

static int mb_bwss_ssl_wait (SSL *ssl, int want, volatile int *running,
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

static int mb_bwss_ssl_accept_while (SSL *ssl, volatile int *running,
    int timeout_ms)
{
    int budget = timeout_ms > 0 ? timeout_ms : 5000;

    for (;;) {
        int rc;
        int err;

        if (running && !*running)
            return -ECANCELED;

        rc = SSL_accept (ssl);
        if (rc == 1)
            return 0;

        err = SSL_get_error (ssl, rc);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
            if (mb_bwss_ssl_wait (ssl, err, running, &budget) < 0)
                return -1;
            continue;
        }
        return -1;
    }
}

static int mb_bwss_ssl_read_http (SSL *ssl, char *buf, size_t buflen,
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
                return (int) pos;
            continue;
        }
        {
            int err = SSL_get_error (ssl, nr);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                if (mb_bwss_ssl_wait (ssl, err, running, budget) < 0)
                    return -1;
                continue;
            }
            return -1;
        }
    }
    return -1;
}

static int mb_bwss_ssl_write_all (SSL *ssl, const void *data, size_t len,
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
                if (mb_bwss_ssl_wait (ssl, err, running, budget) < 0)
                    return -1;
                continue;
            }
            return -1;
        }
    }
    return 0;
}

static int mb_bwss_do_handshake (SSL *ssl, volatile int *running,
    int timeout_ms)
{
    char req[4096];
    char *key;
    char *key_end;
    uint8_t hash_input[128];
    size_t key_len;
    uint8_t hash[20];
    char hash_b64[32];
    char resp[512];
    int budget = timeout_ms > 0 ? timeout_ms : 5000;

    int reqlen = mb_bwss_ssl_read_http (ssl, req, sizeof (req), running,
        &budget);
    if (reqlen < 0)
        return -1;

    key = mb_wss_find_header (req, "Sec-WebSocket-Key:");
    if (!key)
        return -1;

    key_end = strstr (key, "\r\n");
    if (!key_end)
        return -1;
    key_len = (size_t) (key_end - key);

    if (key_len + sizeof (mb_wss_accept_key) - 1 > sizeof (hash_input))
        return -1;

    memcpy (hash_input, key, key_len);
    memcpy (hash_input + key_len, mb_wss_accept_key,
        sizeof (mb_wss_accept_key) - 1);
    mb_sha1 (hash_input, key_len + sizeof (mb_wss_accept_key) - 1, hash);
    mb_b64_encode (hash, 20, hash_b64);

    snprintf (resp, sizeof (resp),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n", hash_b64);

    return mb_bwss_ssl_write_all (ssl, resp, strlen (resp), running, &budget);
}

static void mb_bwss_accept_loop (void *arg)
{
    struct mb_bwss *self = (struct mb_bwss *) arg;

    while (self->running) {
        struct pollfd pfd;
        int rc;

        mb_mutex_lock (&self->lock);
        mb_bwss_free_zombies (self);
        mb_mutex_unlock (&self->lock);

        pfd.fd = self->listen_fd;
        pfd.events = POLLIN;
        rc = poll (&pfd, 1, 100);
        if (rc <= 0)
            continue;
        if (!self->running || self->listen_fd < 0)
            continue;

        if (pfd.revents & POLLIN) {
            struct sockaddr_storage client;
            socklen_t client_len = sizeof (client);
            int client_fd;
            SSL *ssl;
            struct mb_sws *sws;
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

            fcntl (client_fd, F_SETFL,
                fcntl (client_fd, F_GETFL, 0) | O_NONBLOCK);

            SSL_set_fd (ssl, client_fd);

            if (mb_bwss_ssl_accept_while (ssl, &self->running, 5000) < 0) {
                SSL_free (ssl);
                close (client_fd);
                continue;
            }

            if (mb_bwss_do_handshake (ssl, &self->running, 5000) < 0) {
                SSL_free (ssl);
                close (client_fd);
                continue;
            }

            sws = (struct mb_sws *) mb_alloc (sizeof (struct mb_sws));
            if (!sws) {
                SSL_free (ssl);
                close (client_fd);
                continue;
            }

            mb_sws_create (sws, self->ep, client_fd, 0);
            sws->ssl = ssl;
            mb_sws_set_on_error (sws, mb_bwss_on_session_error, self);

            mb_mutex_lock (&self->lock);
            if (mb_sws_start (sws) < 0) {
                mb_sws_term (sws);
                mb_free (sws);
                mb_mutex_unlock (&self->lock);
                continue;
            }
            mb_list_insert (&self->sws_list, &sws->item,
                mb_list_end (&self->sws_list));
            mb_mutex_unlock (&self->lock);
        }
    }
}

int mb_bwss_create (struct mb_ep *ep)
{
    struct mb_bwss *self;
    int fd;
    int rc;
    char host[256];
    uint16_t port;

    rc = mb_net_parse_addr (mb_ep_getaddr (ep), host, sizeof (host), &port);
    if (rc < 0)
        return rc;

    fd = mb_net_bind (host, port, 10, ep->options.ipv4only);
    if (fd < 0)
        return fd;

    self = (struct mb_bwss *) mb_alloc (sizeof (struct mb_bwss));
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
    }

    mb_list_init (&self->sws_list);
    mb_list_init (&self->zombies);
    mb_mutex_init (&self->lock);
    self->running = 1;

    mb_ep_tran_setup (ep, &mb_bwss_ops, self);

    mb_thread_init (&self->accept_thread);
    if (mb_thread_start (&self->accept_thread, mb_bwss_accept_loop, self) != 0) {
        self->running = 0;
        if (self->ctx) {
            SSL_CTX_free (self->ctx);
            self->ctx = NULL;
        }
        close (self->listen_fd);
        self->listen_fd = -1;
        mb_mutex_term (&self->lock);
        mb_list_term (&self->sws_list);
        mb_list_term (&self->zombies);
        mb_free (self);
        return -EAGAIN;
    }

    return 0;
}

static void mb_bwss_stop (void *p)
{
    struct mb_bwss *self = (struct mb_bwss *) p;
    struct mb_list_item *it;
    struct mb_list_item *next;

    self->running = 0;
    if (self->listen_fd >= 0) {
        close (self->listen_fd);
        self->listen_fd = -1;
    }
    mb_thread_term (&self->accept_thread);

    mb_mutex_lock (&self->lock);
    for (it = mb_list_begin (&self->sws_list); it != NULL; it = next) {
        struct mb_sws *sws = mb_cont (it, struct mb_sws, item);
        next = mb_list_next (&self->sws_list, it);
        mb_sws_stop (sws);
        mb_sws_term (sws);
        mb_free (sws);
    }
    mb_list_init (&self->sws_list);
    mb_bwss_free_zombies (self);
    mb_mutex_unlock (&self->lock);

    mb_ep_stopped (self->ep);
}

static void mb_bwss_destroy (void *p)
{
    struct mb_bwss *self = (struct mb_bwss *) p;

    if (self->running)
        mb_bwss_stop (p);

    if (self->listen_fd >= 0) {
        close (self->listen_fd);
        self->listen_fd = -1;
    }
    if (self->ctx)
        SSL_CTX_free (self->ctx);
    mb_mutex_term (&self->lock);
    mb_list_term (&self->sws_list);
    mb_list_term (&self->zombies);
    mb_free (self);
}
