#ifndef MB_TRANSPORT_TLS_STLS_H_INCLUDED
#define MB_TRANSPORT_TLS_STLS_H_INCLUDED

#include "../../transport.h"
#include "../../utils/list.h"

#include <stddef.h>
#include <openssl/ssl.h>

struct mb_ep;

#define MB_STLS_HDR_SIZE 4

struct mb_stls {
    SSL *ssl;
    struct mb_pipebase pipebase;
    struct mb_list_item item;
    uint8_t inhdr[MB_STLS_HDR_SIZE];
    struct mb_msg inmsg;
    int inpos;
    int inlen;
    int instate;
};

int mb_stls_create (struct mb_stls *self, struct mb_ep *ep, SSL *ssl);
void mb_stls_term (struct mb_stls *self);
void mb_stls_start (struct mb_stls *self);
void mb_stls_stop (struct mb_stls *self);

#endif
