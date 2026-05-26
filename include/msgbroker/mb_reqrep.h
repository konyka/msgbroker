#ifndef MB_REQREP_H_INCLUDED
#define MB_REQREP_H_INCLUDED

#include "mb.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Protocol identifier for request/response pattern. */
#define MB_PROTO_REQREP 3

/** MB_REQ/MB_REP are the standard request/response socket types. */
#define MB_REQ (MB_PROTO_REQREP * 16 + 0)
#define MB_REP (MB_PROTO_REQREP * 16 + 1)
/** MB_XREQ/MB_XREP are the extended (XP) versions of request/response. */
#define MB_XREQ (MB_PROTO_REQREP * 16 + 8)
#define MB_XREP (MB_PROTO_REQREP * 16 + 9)

#define MB_REQ_RESEND_IVL 1

#ifdef __cplusplus
}
#endif

#endif
