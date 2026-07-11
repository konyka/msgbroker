#ifndef MB_TRANSPORT_WS_SWS_H_INCLUDED
#define MB_TRANSPORT_WS_SWS_H_INCLUDED

#include "../../transport.h"
#include "ws.h"
#include "../../utils/list.h"

#include <stddef.h>

struct mb_ep;
struct ssl_st;

struct mb_sws {
    int fd;
    int is_client;
    struct mb_pipebase pipebase;
    struct mb_list_item item;
    uint8_t inhdr[MB_WS_MAX_HDR_SIZE];
    struct mb_msg inmsg;
    int inpos;
    int inlen;
    int instate;
    int payload_len;
    int payload_offset;
    uint8_t mask_key[4];
    uint8_t *outbuf;
    size_t outlen;
    size_t outpos;
    struct ssl_st *ssl;
    int disconnected;
    void (*on_error) (void *arg);
    void *on_error_arg;
};

int mb_sws_create (struct mb_sws *self, struct mb_ep *ep, int fd,
    int is_client);
void mb_sws_term (struct mb_sws *self);
void mb_sws_start (struct mb_sws *self);
void mb_sws_stop (struct mb_sws *self);
void mb_sws_set_on_error (struct mb_sws *self, void (*cb) (void *), void *arg);

#endif
