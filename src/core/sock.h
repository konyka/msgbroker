#ifndef MB_CORE_SOCK_H_INCLUDED
#define MB_CORE_SOCK_H_INCLUDED

#include "../protocol.h"
#include "../transport.h"
#include "../aio/ctx.h"
#include "../aio/fsm.h"
#include "../pal/efd.h"
#include "../pal/semaphore.h"
#include "../utils/list.h"

#include <stdint.h>

#define MB_SOCK_STATE_INIT          1
#define MB_SOCK_STATE_ACTIVE        2
#define MB_SOCK_STATE_STOPPING_EPS  3
#define MB_SOCK_STATE_STOPPING      4

#define MB_SOCK_FLAG_IN  1
#define MB_SOCK_FLAG_OUT 2
#define MB_SOCK_FLAG_STOPPING 4

struct mb_sock {
    struct mb_fsm fsm;
    int state;

    struct mb_sockbase *sockbase;
    const struct mb_socktype *socktype;
    int flags;

    struct mb_ctx ctx;
    struct mb_efd sndfd;
    struct mb_efd rcvfd;
    struct mb_sem termsem;

    struct mb_list eps;
    struct mb_list sdeps;
    int eid;
    int holds;

    int sndbuf;
    int rcvbuf;
    int rcvmaxsize;
    int sndtimeo;
    int rcvtimeo;
    int reconnect_ivl;
    int reconnect_ivl_max;
    int maxttl;
    int linger;

    struct mb_ep_options ep_template;
    struct mb_optset *optsets[MB_MAX_TRANSPORT];

    struct {
        uint64_t established_connections;
        uint64_t accepted_connections;
        uint64_t dropped_connections;
        uint64_t broken_connections;
        uint64_t connect_errors;
        uint64_t bind_errors;
        uint64_t accept_errors;
        uint64_t messages_sent;
        uint64_t messages_received;
        uint64_t bytes_sent;
        uint64_t bytes_received;
        int current_connections;
        int inprogress_connections;
        int current_snd_priority;
        int current_ep_errors;
    } statistics;

    char socket_name[64];

    char tls_cert_path[256];
    char tls_key_path[256];
    char tls_ca_path[256];
    int tls_verify;
};

int mb_sock_init (struct mb_sock *self, const struct mb_socktype *socktype,
    int fd);
void mb_sock_stop (struct mb_sock *self);
int mb_sock_term (struct mb_sock *self);
void mb_sock_stopped (struct mb_sock *self);
struct mb_ctx *mb_sock_getctx (struct mb_sock *self);
int mb_sock_ispeer (struct mb_sock *self, int socktype);
int mb_sock_setopt (struct mb_sock *self, int level, int option,
    const void *optval, size_t optvallen);
int mb_sock_getopt (struct mb_sock *self, int level, int option,
    void *optval, size_t *optvallen);
void mb_sock_getopt_inner (struct mb_sock *self, int level, int option,
    void *optval, size_t *optvallen);
int mb_sock_send (struct mb_sock *self, struct mb_msg *msg);
int mb_sock_recv (struct mb_sock *self, struct mb_msg *msg);
int mb_sock_add (struct mb_sock *self, struct mb_ep *ep);
void mb_sock_rm (struct mb_sock *self, struct mb_ep *ep);
int mb_sock_pipe_add (struct mb_sock *self, struct mb_pipe *pipe);
void mb_sock_pipe_rm (struct mb_sock *self, struct mb_pipe *pipe);
void mb_sock_stat_increment (struct mb_sock *self, int name, int increment);
uint64_t mb_sock_get_statistic (struct mb_sock *self, int stat);
int mb_sock_hold (struct mb_sock *self);
void mb_sock_rele (struct mb_sock *self);

#endif
