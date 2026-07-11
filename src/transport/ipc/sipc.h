#ifndef MB_TRANSPORT_IPC_SIPC_H_INCLUDED
#define MB_TRANSPORT_IPC_SIPC_H_INCLUDED

#include "../../transport.h"
#include "../../utils/list.h"

#include <stddef.h>

struct mb_ep;

#define MB_SIPC_HDR_SIZE 4

struct mb_sipc {
    int fd;
    struct mb_pipebase pipebase;
    struct mb_list_item item;
    uint8_t inhdr[MB_SIPC_HDR_SIZE];
    struct mb_msg inmsg;
    uint8_t *outbuf;
    int inpos;
    int inlen;
    int instate;
    int outpos;
    int outlen;
    int disconnected;
    void (*on_error) (void *arg);
    void *on_error_arg;
};

int mb_sipc_create (struct mb_sipc *self, struct mb_ep *ep, int fd);
void mb_sipc_term (struct mb_sipc *self);
int mb_sipc_start (struct mb_sipc *self);
void mb_sipc_stop (struct mb_sipc *self);
void mb_sipc_set_on_error (struct mb_sipc *self, void (*cb) (void *), void *arg);

#endif
