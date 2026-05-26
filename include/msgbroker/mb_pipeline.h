#ifndef MB_PIPELINE_H_INCLUDED
#define MB_PIPELINE_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#define MB_PROTO_PIPELINE 5

#define MB_PUSH (MB_PROTO_PIPELINE * 16 + 0)
#define MB_PULL (MB_PROTO_PIPELINE * 16 + 1)
/** MB_PROTO_PIPELINE constants for push/pull pipeline ops. */
#define MB_XPUSH (MB_PROTO_PIPELINE * 16 + 8)
/** MB_PROTO_PIPELINE's XPULL operation code. */
#define MB_XPULL (MB_PROTO_PIPELINE * 16 + 9)

#ifdef __cplusplus
}
#endif

#endif
