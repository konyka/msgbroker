#ifndef MB_REQREP_H_INCLUDED
#define MB_REQREP_H_INCLUDED

#include "mb.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MB_PROTO_REQREP 3

#define MB_REQ (MB_PROTO_REQREP * 16 + 0)
#define MB_REP (MB_PROTO_REQREP * 16 + 1)
#define MB_XREQ (MB_PROTO_REQREP * 16 + 8)
#define MB_XREP (MB_PROTO_REQREP * 16 + 9)

#define MB_REQ_RESEND_IVL 1

#ifdef __cplusplus
}
#endif

#endif
