#include "stls.h"
#include "../../core/ep.h"
#include "../../core/sock.h"
#include "../../utils/alloc.h"
#include "../../utils/cont.h"
#include "../../utils/err.h"
#include "../../utils/wire.h"
#include "../../memory/msg.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#define MB_STLS_INSTATE_HDR  1
#define MB_STLS_INSTATE_BODY 2
#define MB_STLS_INSTATE_HASMSG 3

static int mb_stls_send (struct mb_pipebase *base, struct mb_msg *msg);
static int mb_stls_recv (struct mb_pipebase *base, struct mb_msg *msg);

static const struct mb_pipebase_vfptr mb_stls_vfptr = {
    mb_stls_send,
    mb_stls_recv,
};

static int mb_stls_send_ssl (SSL *ssl, const void *buf, size_t len)
{
    const uint8_t *ptr = (const uint8_t *) buf;
    size_t remaining = len;
    size_t sent = 0;

    while (remaining > 0) {
        int ns = SSL_write (ssl, ptr, (int) remaining);
        if (ns <= 0) {
            int err = SSL_get_error (ssl, ns);
            if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ)
                return sent > 0 ? (int) sent : -EAGAIN;
            return -ECONNRESET;
        }
        ptr += ns;
        remaining -= (size_t) ns;
        sent += (size_t) ns;
    }
    return (int) sent;
}

static int mb_stls_recv_ssl (SSL *ssl, void *buf, size_t len)
{
    uint8_t *ptr = (uint8_t *) buf;
    size_t remaining = len;
    size_t got = 0;

    while (remaining > 0) {
        int nr = SSL_read (ssl, ptr, (int) remaining);
        if (nr <= 0) {
            int err = SSL_get_error (ssl, nr);
            if (err == SSL_ERROR_ZERO_RETURN)
                return got > 0 ? (int) got : -ECONNRESET;
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
                return got > 0 ? (int) got : -EAGAIN;
            return -ECONNRESET;
        }
        ptr += nr;
        remaining -= (size_t) nr;
        got += (size_t) nr;
    }
    return (int) got;
}

int mb_stls_create (struct mb_stls *self, struct mb_ep *ep, SSL *ssl)
{
    self->ssl = ssl;
    mb_pipebase_init (&self->pipebase, &mb_stls_vfptr, ep);
    mb_list_item_init (&self->item);
    self->inpos = 0;
    self->inlen = 0;
    self->instate = MB_STLS_INSTATE_HDR;
    self->outbuf = NULL;
    self->outpos = 0;
    self->outlen = 0;
    self->disconnected = 0;
    self->on_error = NULL;
    self->on_error_arg = NULL;
    mb_msg_init (&self->inmsg, 0);
    return 0;
}

void mb_stls_set_on_error (struct mb_stls *self, void (*cb) (void *), void *arg)
{
    self->on_error = cb;
    self->on_error_arg = arg;
}

static void mb_stls_report_error (struct mb_stls *self)
{
    void (*cb) (void *);
    void *arg;

    if (self->disconnected)
        return;
    self->disconnected = 1;
    cb = self->on_error;
    arg = self->on_error_arg;
    self->on_error = NULL;
    if (cb)
        cb (arg);
}

void mb_stls_term (struct mb_stls *self)
{
    mb_msg_term (&self->inmsg);
    mb_list_item_term (&self->item);
    mb_pipebase_term (&self->pipebase);
    if (self->outbuf) {
        mb_free (self->outbuf);
        self->outbuf = NULL;
    }
    if (self->ssl) {
        int fd = SSL_get_fd (self->ssl);
        SSL_shutdown (self->ssl);
        SSL_free (self->ssl);
        self->ssl = NULL;
        if (fd >= 0)
            close (fd);
    }
}

void mb_stls_start (struct mb_stls *self)
{
    mb_pipebase_start (&self->pipebase);
}

void mb_stls_stop (struct mb_stls *self)
{
    if (self->pipebase.state == 2)
        mb_pipebase_stop (&self->pipebase);
    if (self->outbuf) {
        mb_free (self->outbuf);
        self->outbuf = NULL;
        self->outpos = 0;
        self->outlen = 0;
    }
    if (self->ssl) {
        int fd = SSL_get_fd (self->ssl);
        SSL_shutdown (self->ssl);
        SSL_free (self->ssl);
        self->ssl = NULL;
        if (fd >= 0)
            close (fd);
    }
}

static int mb_stls_send (struct mb_pipebase *base, struct mb_msg *msg)
{
    struct mb_stls *self = mb_cont (base, struct mb_stls, pipebase);
    int rc;

    if (!self->outbuf) {
        size_t body_size = mb_chunkref_size (&msg->body);

        self->outlen = (int) (MB_STLS_HDR_SIZE + body_size);
        self->outbuf = (uint8_t *) mb_alloc ((size_t) self->outlen);
        if (!self->outbuf)
            return -ENOMEM;
        mb_wire_put_uint32 (self->outbuf, (uint32_t) body_size);
        if (body_size > 0)
            memcpy (self->outbuf + MB_STLS_HDR_SIZE,
                mb_chunkref_data (&msg->body), body_size);
        self->outpos = 0;
    }

    while (self->outpos < self->outlen) {
        rc = mb_stls_send_ssl (self->ssl, self->outbuf + self->outpos,
            (size_t) (self->outlen - self->outpos));
        if (rc < 0) {
            if (rc != -EAGAIN)
                mb_stls_report_error (self);
            return rc;
        }
        self->outpos += rc;
    }

    mb_free (self->outbuf);
    self->outbuf = NULL;
    self->outpos = 0;
    self->outlen = 0;

    mb_msg_term (msg);
    mb_msg_init (msg, 0);
    return 0;
}

static int mb_stls_recv (struct mb_pipebase *base, struct mb_msg *msg)
{
    struct mb_stls *self = mb_cont (base, struct mb_stls, pipebase);
    int rc;

    if (self->instate == MB_STLS_INSTATE_HASMSG) {
        mb_msg_mv (msg, &self->inmsg);
        mb_msg_init (&self->inmsg, 0);
        self->instate = MB_STLS_INSTATE_HDR;
        self->inpos = 0;
        return 0;
    }

    if (self->instate == MB_STLS_INSTATE_HDR) {
        rc = mb_stls_recv_ssl (self->ssl,
            self->inhdr + self->inpos,
            MB_STLS_HDR_SIZE - self->inpos);
        if (rc < 0) {
            if (rc != -EAGAIN)
                mb_stls_report_error (self);
            return rc;
        }
        self->inpos += rc;
        if (self->inpos < MB_STLS_HDR_SIZE)
            return -EAGAIN;

        self->inlen = (int) mb_wire_get_uint32 (self->inhdr);
        if (self->inlen < 0 || self->inlen > 1024 * 1024) {
            mb_stls_report_error (self);
            return -EMSGSIZE;
        }
        self->inpos = 0;
        self->instate = MB_STLS_INSTATE_BODY;

        mb_msg_term (&self->inmsg);
        mb_msg_init (&self->inmsg, (size_t) self->inlen);
    }

    if (self->instate == MB_STLS_INSTATE_BODY) {
        void *body;

        if (self->inlen > 0) {
            body = mb_chunkref_data (&self->inmsg.body);
            rc = mb_stls_recv_ssl (self->ssl,
                (uint8_t *) body + self->inpos,
                (size_t) self->inlen - self->inpos);
            if (rc < 0) {
                if (rc != -EAGAIN)
                    mb_stls_report_error (self);
                return rc;
            }
            self->inpos += rc;
            if (self->inpos < self->inlen)
                return -EAGAIN;
        }

        self->instate = MB_STLS_INSTATE_HASMSG;
        mb_msg_mv (msg, &self->inmsg);
        mb_msg_init (&self->inmsg, 0);
        self->instate = MB_STLS_INSTATE_HDR;
        self->inpos = 0;
    }

    return 0;
}
