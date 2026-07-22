#include "sws.h"
#include "ws.h"
#include "../../core/ep.h"
#include "../../core/sock.h"
#include "../../pal/efd.h"
#include "../../pal/clock.h"
#include "../../utils/alloc.h"
#include "../../utils/cont.h"
#include "../../utils/err.h"
#include "../../utils/net.h"
#include "../../utils/wire.h"
#include "../../memory/msg.h"
#include "../../memory/chunk.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <poll.h>
#include <openssl/ssl.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define MB_SWS_INSTATE_HDR  1
#define MB_SWS_INSTATE_BODY 2
#define MB_SWS_INSTATE_HASMSG 3
#define MB_SWS_INSTATE_CTRL 4

static int mb_sws_send (struct mb_pipebase *base, struct mb_msg *msg);
static int mb_sws_recv (struct mb_pipebase *base, struct mb_msg *msg);
static int mb_sws_has_msg (struct mb_pipebase *base);
static int mb_sws_can_send (struct mb_pipebase *base);
static int mb_sws_flush_outbuf (struct mb_sws *self);

static const struct mb_pipebase_vfptr mb_sws_vfptr = {
    mb_sws_send,
    mb_sws_recv,
    mb_sws_has_msg,
    mb_sws_can_send,
};

static int mb_sws_has_msg (struct mb_pipebase *base)
{
    struct mb_sws *self = mb_cont (base, struct mb_sws, pipebase);
    struct pollfd pfd;
    int rc;

    if (self->instate == MB_SWS_INSTATE_HASMSG)
        return 1;
    if (self->fd < 0 || self->disconnected)
        return 0;
    if (self->ssl && SSL_pending (self->ssl) > 0)
        return 1;

    pfd.fd = self->fd;
    pfd.events = POLLIN;
    rc = poll (&pfd, 1, 0);
    return rc > 0 && (pfd.revents & (POLLIN | POLLHUP | POLLERR)) != 0;
}

static int mb_sws_can_send (struct mb_pipebase *base)
{
    struct mb_sws *self = mb_cont (base, struct mb_sws, pipebase);
    struct pollfd pfd;
    int rc;

    if (self->fd < 0 || self->disconnected)
        return 0;
    if (!self->outbuf)
        return 1;

    pfd.fd = self->fd;
    pfd.events = POLLOUT;
    rc = poll (&pfd, 1, 0);
    if (rc <= 0 || !(pfd.revents & POLLOUT))
        return 0;
    rc = mb_sws_flush_outbuf (self);
    if (rc < 0 && rc != -EAGAIN)
        return 0;
    return self->outbuf == NULL;
}

static void mb_ws_mask (uint8_t *data, size_t len, const uint8_t *key)
{
    size_t i;
    for (i = 0; i < len; ++i)
        data[i] ^= key[i % 4];
}

static int mb_sws_do_recv (struct mb_sws *self, void *buf, size_t len,
    ssize_t *out)
{
    if (self->ssl) {
        int nr = SSL_read (self->ssl, buf, (int) len);
        if (nr <= 0) {
            int err = SSL_get_error (self->ssl, nr);
            if (err == SSL_ERROR_ZERO_RETURN) {
                *out = 0;
                return 0;
            }
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                *out = -1;
                errno = EAGAIN;
                return 0;
            }
            *out = 0;
            return 0;
        }
        *out = (ssize_t) nr;
        return 0;
    }
    *out = recv (self->fd, buf, len, 0);
    return 0;
}

static int mb_sws_send_raw (struct mb_sws *self, const void *buf, size_t len)
{
    const uint8_t *ptr = (const uint8_t *) buf;
    size_t remaining = len;
    size_t sent = 0;

    while (remaining > 0) {
        ssize_t ns;
        if (self->ssl) {
            ns = SSL_write (self->ssl, ptr, (int) remaining);
            if (ns <= 0) {
                int err = SSL_get_error (self->ssl, (int) ns);
                if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ)
                    return sent > 0 ? (int) sent : -EAGAIN;
                return -ECONNRESET;
            }
        } else {
            ns = send (self->fd, ptr, remaining, MSG_NOSIGNAL);
            if (ns < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    return sent > 0 ? (int) sent : -EAGAIN;
                return -errno;
            }
        }
        ptr += ns;
        remaining -= (size_t) ns;
        sent += (size_t) ns;
    }
    return (int) sent;
}

static int mb_sws_send_all (struct mb_sws *self, const void *buf, size_t len)
{
    const uint8_t *ptr = (const uint8_t *) buf;
    size_t pos = 0;

    while (pos < len) {
        int rc = mb_sws_send_raw (self, ptr + pos, len - pos);
        if (rc < 0)
            return rc;
        pos += (size_t) rc;
    }
    return 0;
}

static int mb_sws_build_frame (struct mb_sws *self, int opcode,
    const void *data, size_t len, uint8_t **out_buf, size_t *out_len)
{
    uint8_t hdr[MB_WS_MAX_HDR_SIZE];
    int hdrlen = 0;
    size_t total;
    uint8_t *frame;
    uint8_t mask[4];

    hdr[0] = MB_WS_FIN_BIT | (uint8_t) opcode;

    if (len < 126) {
        hdr[1] = (uint8_t) len;
        if (self->is_client)
            hdr[1] |= MB_WS_MASK_BIT;
        hdrlen = 2;
    } else if (len <= 65535) {
        hdr[1] = 126;
        if (self->is_client)
            hdr[1] |= MB_WS_MASK_BIT;
        hdr[2] = (uint8_t) ((len >> 8) & 0xFF);
        hdr[3] = (uint8_t) (len & 0xFF);
        hdrlen = 4;
    } else {
        hdr[1] = 127;
        if (self->is_client)
            hdr[1] |= MB_WS_MASK_BIT;
        hdr[2] = 0; hdr[3] = 0; hdr[4] = 0; hdr[5] = 0;
        hdr[6] = (uint8_t) ((len >> 24) & 0xFF);
        hdr[7] = (uint8_t) ((len >> 16) & 0xFF);
        hdr[8] = (uint8_t) ((len >> 8) & 0xFF);
        hdr[9] = (uint8_t) (len & 0xFF);
        hdrlen = 10;
    }

    if (self->is_client) {
        mask[0] = (uint8_t) (rand () & 0xFF);
        mask[1] = (uint8_t) (rand () & 0xFF);
        mask[2] = (uint8_t) (rand () & 0xFF);
        mask[3] = (uint8_t) (rand () & 0xFF);
        hdr[hdrlen++] = mask[0];
        hdr[hdrlen++] = mask[1];
        hdr[hdrlen++] = mask[2];
        hdr[hdrlen++] = mask[3];
    }

    total = (size_t) hdrlen + len;
    frame = (uint8_t *) mb_alloc (total);
    if (!frame)
        return -ENOMEM;

    memcpy (frame, hdr, (size_t) hdrlen);
    if (len > 0) {
        memcpy (frame + hdrlen, data, len);
        if (self->is_client)
            mb_ws_mask (frame + hdrlen, len, mask);
    }

    *out_buf = frame;
    *out_len = total;
    return 0;
}

static int mb_sws_send_frame (struct mb_sws *self, int opcode,
    const void *data, size_t len)
{
    uint8_t *frame;
    size_t frame_len;
    int rc;

    rc = mb_sws_build_frame (self, opcode, data, len, &frame, &frame_len);
    if (rc < 0)
        return rc;

    rc = mb_sws_send_all (self, frame, frame_len);
    mb_free (frame);
    return rc;
}

static int mb_sws_send_pong (struct mb_sws *self, const void *data,
    size_t len)
{
    return mb_sws_send_frame (self, MB_WS_OPCODE_PONG, data, len);
}

int mb_sws_create (struct mb_sws *self, struct mb_ep *ep, int fd,
    int is_client)
{
    struct mb_sock *sock;

    self->fd = fd;
    self->is_client = is_client;
    self->ssl = NULL;
    mb_pipebase_init (&self->pipebase, &mb_sws_vfptr, ep);
    sock = mb_ep_sock (ep);
    mb_net_apply_bufs (fd, sock->sndbuf, sock->rcvbuf);
    mb_list_item_init (&self->item);
    self->inpos = 0;
    self->inlen = 0;
    self->instate = MB_SWS_INSTATE_HDR;
    self->payload_len = 0;
    self->payload_offset = 0;
    self->outbuf = NULL;
    self->outlen = 0;
    self->outpos = 0;
    self->pending_pong = 0;
    self->pong_len = 0;
    memset (self->mask_key, 0, 4);
    self->disconnected = 0;
    self->on_error = NULL;
    self->on_error_arg = NULL;
    mb_msg_init (&self->inmsg, 0);
    return 0;
}

void mb_sws_set_on_error (struct mb_sws *self, void (*cb) (void *), void *arg)
{
    self->on_error = cb;
    self->on_error_arg = arg;
}

static void mb_sws_report_error (struct mb_sws *self)
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

void mb_sws_term (struct mb_sws *self)
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
        self->fd = -1;
    } else if (self->fd >= 0) {
        close (self->fd);
        self->fd = -1;
    }
}

int mb_sws_start (struct mb_sws *self)
{
    return mb_pipebase_start (&self->pipebase);
}

static int mb_sws_flush_outbuf (struct mb_sws *self)
{
    int rc;

    if (!self->outbuf)
        return 0;
    if (self->fd < 0 && !self->ssl)
        return -ECONNRESET;

    while (self->outpos < self->outlen) {
        rc = mb_sws_send_raw (self, self->outbuf + self->outpos,
            self->outlen - self->outpos);
        if (rc < 0) {
            if (rc != -EAGAIN)
                mb_sws_report_error (self);
            return rc;
        }
        self->outpos += (size_t) rc;
    }
    mb_free (self->outbuf);
    self->outbuf = NULL;
    self->outlen = 0;
    self->outpos = 0;
    return 0;
}

static void mb_sws_linger_flush (struct mb_sws *self)
{
    int linger;
    int fd;
    uint64_t deadline;
    struct pollfd pfd;

    if (!self->outbuf)
        return;

    linger = self->pipebase.sock ? self->pipebase.sock->linger : 0;
    if (linger <= 0)
        return;

    fd = self->ssl ? SSL_get_fd (self->ssl) : self->fd;
    if (fd < 0)
        return;

    deadline = mb_clock_ms () + (uint64_t) linger;
    while (self->outbuf) {
        int64_t left = (int64_t) deadline - (int64_t) mb_clock_ms ();
        int rc;

        if (left <= 0)
            break;
        pfd.fd = fd;
        pfd.events = POLLOUT;
        rc = poll (&pfd, 1, (int) left);
        if (rc <= 0)
            break;
        rc = mb_sws_flush_outbuf (self);
        if (rc != -EAGAIN)
            break;
    }
}

void mb_sws_stop (struct mb_sws *self)
{
    if (self->pipebase.state == 2)
        mb_pipebase_stop (&self->pipebase);
    mb_sws_linger_flush (self);
    if (self->outbuf) {
        mb_free (self->outbuf);
        self->outbuf = NULL;
        self->outlen = 0;
        self->outpos = 0;
    }
    if (self->ssl) {
        int fd = SSL_get_fd (self->ssl);
        SSL_shutdown (self->ssl);
        SSL_free (self->ssl);
        self->ssl = NULL;
        if (fd >= 0)
            close (fd);
        self->fd = -1;
    } else if (self->fd >= 0) {
        close (self->fd);
        self->fd = -1;
    }
}

static int mb_sws_flush_pending_pong (struct mb_sws *self)
{
    int rc;

    if (!self->pending_pong)
        return 0;

    rc = mb_sws_send_pong (self, self->pong_buf, (size_t) self->pong_len);
    if (rc < 0) {
        if (rc == -EAGAIN)
            return -EAGAIN;
        self->pending_pong = 0;
        self->pong_len = 0;
        mb_sws_report_error (self);
        return rc;
    }
    self->pending_pong = 0;
    self->pong_len = 0;
    return 0;
}

static int mb_sws_send (struct mb_pipebase *base, struct mb_msg *msg)
{
    struct mb_sws *self = mb_cont (base, struct mb_sws, pipebase);
    uint8_t hdr[4];
    size_t body_len;
    int rc;

    if (self->fd < 0 && !self->ssl) {
        mb_sws_report_error (self);
        return -ECONNRESET;
    }

    if (!self->outbuf && self->pending_pong) {
        rc = mb_sws_flush_pending_pong (self);
        if (rc < 0) {
            if (rc != -EAGAIN)
                return rc;
            return -EAGAIN;
        }
    }

    rc = mb_sws_flush_outbuf (self);
    if (rc < 0)
        return rc;

    {
        uint8_t *payload;
        size_t payload_len;

        body_len = mb_chunkref_size (&msg->body);
        if (mb_sock_msg_too_large (base->sock, body_len))
            return -EMSGSIZE;
        mb_wire_put_uint32 (hdr, (uint32_t) body_len);
        payload_len = 4 + body_len;
        payload = (uint8_t *) mb_alloc (payload_len);
        if (!payload)
            return -ENOMEM;

        memcpy (payload, hdr, 4);
        if (body_len > 0) {
            void *body_data = mb_chunkref_data (&msg->body);
            if (body_data)
                memcpy (payload + 4, body_data, body_len);
        }

        rc = mb_sws_build_frame (self, MB_WS_OPCODE_BINARY, payload,
            payload_len, &self->outbuf, &self->outlen);
        mb_free (payload);
        if (rc < 0)
            return rc;
        self->outpos = 0;

        /* Accepted: EAGAIN means pending wire flush, not "msg not taken". */
        mb_msg_term (msg);
        mb_msg_init (msg, 0);
    }

    rc = mb_sws_flush_outbuf (self);
    if (rc == -EAGAIN)
        return 0;
    if (rc < 0)
        return rc;

    (void) mb_sws_flush_pending_pong (self);
    return 0;
}

static int mb_sws_recv (struct mb_pipebase *base, struct mb_msg *msg)
{
    struct mb_sws *self = mb_cont (base, struct mb_sws, pipebase);
    int rc;

    if (self->fd < 0 && !self->ssl) {
        mb_sws_report_error (self);
        return -ECONNRESET;
    }

    /* Best-effort: write backpressure must not block delivery of reads. */
    rc = mb_sws_flush_outbuf (self);
    if (rc < 0 && rc != -EAGAIN)
        return rc;

    if (!self->outbuf)
        (void) mb_sws_flush_pending_pong (self);

    for (;;) {
        if (self->instate == MB_SWS_INSTATE_HDR) {
            int need;

            if (self->inpos < 2) {
                ssize_t nr;
                mb_sws_do_recv (self, self->inhdr + self->inpos,
                    (size_t) (2 - self->inpos), &nr);
                if (nr <= 0) {
                    if (nr == 0) {
                        mb_sws_report_error (self);
                        return -ECONNRESET;
                    }
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        return -EAGAIN;
                    mb_sws_report_error (self);
                    return -errno;
                }
                self->inpos += (int) nr;
            }

            if (self->inpos < 2)
                return -EAGAIN;

            {
                uint8_t payload_len_byte = self->inhdr[1] & 0x7F;
                int masked = (self->inhdr[1] & MB_WS_MASK_BIT) ? 1 : 0;

                need = 2;
                if (payload_len_byte == 126)
                    need += 2;
                else if (payload_len_byte == 127)
                    need += 8;
                if (masked)
                    need += 4;
            }

            while (self->inpos < need) {
                ssize_t nr;
                mb_sws_do_recv (self, self->inhdr + self->inpos,
                    (size_t) (need - self->inpos), &nr);
                if (nr <= 0) {
                    if (nr == 0) {
                        mb_sws_report_error (self);
                        return -ECONNRESET;
                    }
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        return -EAGAIN;
                    mb_sws_report_error (self);
                    return -errno;
                }
                self->inpos += (int) nr;
            }

            {
                uint8_t opcode = self->inhdr[0] & 0x0F;
                uint8_t payload_len_byte = self->inhdr[1] & 0x7F;
                int masked = (self->inhdr[1] & MB_WS_MASK_BIT) ? 1 : 0;
                int hdr_used = 2;

                if (payload_len_byte < 126) {
                    self->payload_len = (int) payload_len_byte;
                } else if (payload_len_byte == 126) {
                    self->payload_len = (int) (
                        ((uint16_t) self->inhdr[2] << 8) |
                        self->inhdr[3]);
                    hdr_used += 2;
                } else {
                    /* RFC 6455: 8-byte length. Reject if >32-bit or oversized. */
                    if (self->inhdr[2] | self->inhdr[3] |
                        self->inhdr[4] | self->inhdr[5]) {
                        mb_sws_report_error (self);
                        return -EMSGSIZE;
                    }
                    self->payload_len = (int) (
                        ((uint32_t) self->inhdr[6] << 24) |
                        ((uint32_t) self->inhdr[7] << 16) |
                        ((uint32_t) self->inhdr[8] << 8) |
                        self->inhdr[9]);
                    hdr_used += 8;
                }

                if (masked) {
                    self->mask_key[0] = self->inhdr[hdr_used];
                    self->mask_key[1] = self->inhdr[hdr_used + 1];
                    self->mask_key[2] = self->inhdr[hdr_used + 2];
                    self->mask_key[3] = self->inhdr[hdr_used + 3];
                }

                if (opcode == MB_WS_OPCODE_CLOSE) {
                    if (self->outbuf) {
                        mb_free (self->outbuf);
                        self->outbuf = NULL;
                        self->outlen = 0;
                        self->outpos = 0;
                    }
                    self->pending_pong = 0;
                    mb_sws_send_frame (self, MB_WS_OPCODE_CLOSE, NULL, 0);
                    mb_sws_report_error (self);
                    return -ECONNRESET;
                }

                if (opcode == MB_WS_OPCODE_PING ||
                    opcode == MB_WS_OPCODE_PONG) {
                    /* Control frames must be <= 125 bytes (RFC 6455). */
                    if (self->payload_len > 125) {
                        mb_sws_report_error (self);
                        return -EPROTO;
                    }
                    mb_msg_term (&self->inmsg);
                    if (mb_msg_init_size (&self->inmsg,
                            (size_t) self->payload_len) < 0) {
                        mb_sws_report_error (self);
                        return -ENOMEM;
                    }
                    self->inlen = (int) opcode;
                    self->inpos = 0;
                    self->instate = MB_SWS_INSTATE_CTRL;
                    continue;
                }

                mb_msg_term (&self->inmsg);
                /* payload = 4-byte length prefix + body */
                if (self->payload_len < 4 ||
                    mb_sock_msg_too_large (base->sock,
                        (size_t) (self->payload_len - 4))) {
                    mb_sws_report_error (self);
                    return -EMSGSIZE;
                }
                if (mb_msg_init_size (&self->inmsg,
                        (size_t) self->payload_len) < 0) {
                    mb_sws_report_error (self);
                    return -ENOMEM;
                }
                self->instate = MB_SWS_INSTATE_BODY;
                self->inpos = 0;
            }
        }

        if (self->instate == MB_SWS_INSTATE_CTRL) {
            int masked = (self->inhdr[1] & MB_WS_MASK_BIT) ? 1 : 0;

            while (self->inpos < self->payload_len) {
                ssize_t nr;
                uint8_t *dst = (uint8_t *) mb_chunkref_data (
                    &self->inmsg.body);
                mb_sws_do_recv (self, dst + self->inpos,
                    (size_t) (self->payload_len - self->inpos), &nr);
                if (nr <= 0) {
                    if (nr == 0) {
                        mb_sws_report_error (self);
                        return -ECONNRESET;
                    }
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        return -EAGAIN;
                    mb_sws_report_error (self);
                    return -errno;
                }
                self->inpos += (int) nr;
            }

            if (self->payload_len > 0 && masked)
                mb_ws_mask (mb_chunkref_data (&self->inmsg.body),
                    (size_t) self->payload_len, self->mask_key);

            if (self->inlen == MB_WS_OPCODE_PING) {
                int prc;

                if (self->payload_len > 0)
                    memcpy (self->pong_buf,
                        mb_chunkref_data (&self->inmsg.body),
                        (size_t) self->payload_len);
                self->pong_len = self->payload_len;
                self->pending_pong = 1;

                if (!self->outbuf) {
                    prc = mb_sws_flush_pending_pong (self);
                    if (prc < 0 && prc != -EAGAIN) {
                        mb_msg_term (&self->inmsg);
                        mb_msg_init (&self->inmsg, 0);
                        self->instate = MB_SWS_INSTATE_HDR;
                        self->inpos = 0;
                        self->payload_len = 0;
                        return prc;
                    }
                }
            }

            mb_msg_term (&self->inmsg);
            mb_msg_init (&self->inmsg, 0);
            self->instate = MB_SWS_INSTATE_HDR;
            self->inpos = 0;
            self->payload_len = 0;
            if (self->pending_pong && !self->outbuf)
                (void) mb_sws_flush_pending_pong (self);
            continue;
        }

        if (self->instate == MB_SWS_INSTATE_BODY) {
            uint8_t *body;

            while (self->inpos < self->payload_len) {
                ssize_t nr;
                body = (uint8_t *) mb_chunkref_data (&self->inmsg.body);
                mb_sws_do_recv (self, body + self->inpos,
                    (size_t) (self->payload_len - self->inpos), &nr);
                if (nr <= 0) {
                    if (nr == 0) {
                        mb_sws_report_error (self);
                        return -ECONNRESET;
                    }
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        return -EAGAIN;
                    mb_sws_report_error (self);
                    return -errno;
                }
                self->inpos += (int) nr;
            }

            body = (uint8_t *) mb_chunkref_data (&self->inmsg.body);

            if ((self->inhdr[1] & MB_WS_MASK_BIT) && self->payload_len > 0)
                mb_ws_mask (body, (size_t) self->payload_len,
                    self->mask_key);

            if (self->payload_len < 4) {
                mb_msg_term (&self->inmsg);
                mb_msg_init (&self->inmsg, 0);
                mb_sws_report_error (self);
                return -EPROTO;
            }

            {
                uint32_t msg_len = mb_wire_get_uint32 (body);
                if (msg_len > (uint32_t) (self->payload_len - 4) ||
                    mb_sock_msg_too_large (base->sock, (size_t) msg_len)) {
                    mb_msg_term (&self->inmsg);
                    mb_msg_init (&self->inmsg, 0);
                    mb_sws_report_error (self);
                    return -EMSGSIZE;
                }

                if (msg_len > 0) {
                    void *chunk = NULL;
                    int arc = mb_chunk_alloc ((size_t) msg_len, &chunk);
                    if (arc < 0) {
                        /* Payload already consumed; cannot recover the stream. */
                        mb_msg_term (&self->inmsg);
                        mb_msg_init (&self->inmsg, 0);
                        self->instate = MB_SWS_INSTATE_HDR;
                        self->inpos = 0;
                        self->payload_len = 0;
                        mb_sws_report_error (self);
                        return arc;
                    }
                    memcpy (chunk, body + 4, (size_t) msg_len);
                    mb_msg_term (&self->inmsg);
                    mb_msg_init_chunk (&self->inmsg, chunk);
                } else {
                    mb_msg_term (&self->inmsg);
                    mb_msg_init (&self->inmsg, 0);
                }
            }

            self->instate = MB_SWS_INSTATE_HASMSG;
        }

        if (self->instate == MB_SWS_INSTATE_HASMSG) {
            mb_msg_mv (msg, &self->inmsg);
            mb_msg_init (&self->inmsg, 0);
            self->instate = MB_SWS_INSTATE_HDR;
            self->inpos = 0;
            return 0;
        }
    }
}
