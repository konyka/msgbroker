#ifndef MB_POOL_H_INCLUDED
#define MB_POOL_H_INCLUDED

#include "worker.h"

struct mb_pool {
    struct mb_worker worker;
    int started;
};

int mb_pool_init (struct mb_pool *self);
void mb_pool_term (struct mb_pool *self);
struct mb_worker *mb_pool_choose_worker (struct mb_pool *self);

#endif
