#ifndef MB_CORE_GLOBAL_H_INCLUDED
#define MB_CORE_GLOBAL_H_INCLUDED

#include "../aio/pool.h"

struct mb_sock;
struct mb_pool;

struct mb_pool *mb_global_pool (void);
struct mb_ctx *mb_global_getctx (void);
const struct mb_transport *mb_global_transport (int id);
int mb_global_hold_socket (struct mb_sock **sockp, int s);
void mb_global_rele_socket (struct mb_sock *sock);

#endif
