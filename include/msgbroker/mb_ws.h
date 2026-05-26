#ifndef MB_WS_H_INCLUDED
#define MB_WS_H_INCLUDED

#include "mb.h"

#ifdef __cplusplus
extern "C" {
#endif

/** WebSocket transport identifier. */
#define MB_WS -4

#define MB_WS_MSG_TYPE 1

#define MB_WS_MSG_TYPE_TEXT   0x01
#define MB_WS_MSG_TYPE_BINARY 0x02

#ifdef __cplusplus
}
#endif

#endif
