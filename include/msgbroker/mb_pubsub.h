#ifndef MB_PUBSUB_H_INCLUDED
#define MB_PUBSUB_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/** Protocol identifier for publish/subscribe pattern. */
#define MB_PROTO_PUBSUB 2

/** Publish/subscribe socket types. */
#define MB_PUB  (MB_PROTO_PUBSUB * 16 + 0)
#define MB_SUB  (MB_PROTO_PUBSUB * 16 + 1)
/** XP variants for PUB/SUB. */
#define MB_XPUB (MB_PROTO_PUBSUB * 16 + 8)
#define MB_XSUB (MB_PROTO_PUBSUB * 16 + 9)

/** Subscribe to a topic prefix; empty (len 0) means all topics. */
#define MB_SUB_SUBSCRIBE   1
#define MB_SUB_UNSUBSCRIBE 2

#define MB_SUB_PROTO MB_PROTO_PUBSUB

#ifdef __cplusplus
}
#endif

#endif
