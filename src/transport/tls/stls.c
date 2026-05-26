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

    while (remaining > 0) {
        int ns = SSL_write (ssl, ptr, (int) remaining);
        if (ns <= 0) {
            int err = SSL_get_error (ssl, ns);
            if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ)
                return -EAGAIN;
            return -ECONNRESET;
        }
        ptr += ns;
        remaining -= (size_t) ns;
    }
    return 0;
}

static int mb_stls_recv_ssl (SSL *ssl, void *buf, size_t len)
{
    uint8_t *ptr = (uint8_t *) buf;
    size_t remaining = len;

    while (remaining > 0) {
        int nr = SSL_read (ssl, ptr, (int) remaining);
        if (nr <= 0) {
            int err = SSL_get_error (ssl, nr);
            if (err == SSL_ERROR_ZERO_RETURN)
                return -ECONNRESET;
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
                return -EAGAIN;
            return -ECONNRESET;
        }
        ptr += nr;
        remaining -= (size_t) nr;
    }
    return 0;
}

int mb_stls_create (struct mb_stls *self, struct mb_ep *ep, SSL *ssl)
{
    self->ssl = ssl;
    mb_pipebase_init (&self->pipebase, &mb_stls_vfptr, ep);
    mb_list_item_init (&self->item);
    self->inpos = 0;
    self->inlen = 0;
    self->instate = MB_STLS_INSTATE_HDR;
    mb_msg_init (&self->inmsg, 0);
    return 0;
}

void mb_stls_term (struct mb_stls *self)
{
    mb_msg_term (&self->inmsg);
    mb_list_item_term (&self->item);
    mb_pipebase_term (&self->pipebase);
    if (self->ssl) {
        SSL_shutdown (self->ssl);
        SSL_free (self->ssl);
        self->ssl = NULL;
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
    if (self->ssl) {
        SSL_shutdown (self->ssl);
        SSL_free (self->ssl);
        self->ssl = NULL;
    }
}

static int mb_stls_send (struct mb_pipebase *base, struct mb_msg *msg)
{
    struct mb_stls *self = mb_cont (base, struct mb_stls, pipebase);
    uint8_t hdr[MB_STLS_HDR_SIZE];
    int rc;
    size_t bodysz;
    void *body;

    bodysz = mb_chunkref_size (&msg->body);
    mb_wire_put_uint32 (hdr, (uint32_t) bodysz);

    rc = mb_stls_send_ssl (self->ssl, hdr, MB_STLS_HDR_SIZE);
    if (rc < 0)
        return rc;

    if (bodysz > 0) {
        body = mb_chunkref_data (&msg->body);
        rc = mb_stls_send_ssl (self->ssl, body, bodysz);
        if (rc < 0)
            return rc;
    }

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
        if (rc < 0)
            return rc;

        self->inlen = (int) mb_wire_get_uint32 (self->inhdr);
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
            if (rc < 0)
                return rc;
        }

        self->instate = MB_STLS_INSTATE_HASMSG;
        mb_msg_mv (msg, &self->inmsg);
        mb_msg_init (&self->inmsg, 0);
        self->instate = MB_STLS_INSTATE_HDR;
        self->inpos = 0;
    }

    return 0;
}
