#ifndef MB_PUBSUB_H_INCLUDED
#define MB_PUBSUB_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#define MB_PROTO_PUBSUB 2

#define MB_PUB  (MB_PROTO_PUBSUB * 16 + 0)
#define MB_SUB  (MB_PROTO_PUBSUB * 16 + 1)
#define MB_XPUB (MB_PROTO_PUBSUB * 16 + 8)
#define MB_XSUB (MB_PROTO_PUBSUB * 16 + 9)

#define MB_SUB_SUBSCRIBE   1
#define MB_SUB_UNSUBSCRIBE 2

#ifdef __cplusplus
}
#endif

#endif
