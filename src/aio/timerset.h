#ifndef MB_TIMERSET_H_INCLUDED
#define MB_TIMERSET_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

struct mb_timerset_hndl;

struct mb_timerset_hndl {
    int timeout;
    uint64_t expiry;
    struct mb_timerset_hndl *prev;
    struct mb_timerset_hndl *next;
};

struct mb_timerset {
    struct mb_timerset_hndl *head;
};

void mb_timerset_init (struct mb_timerset *self);
void mb_timerset_term (struct mb_timerset *self);
int mb_timerset_timeout (struct mb_timerset *self);
void mb_timerset_insert (struct mb_timerset *self,
    struct mb_timerset_hndl *hndl);
void mb_timerset_cancel (struct mb_timerset_hndl *hndl);
void mb_timerset_tick (struct mb_timerset *self);

#endif
