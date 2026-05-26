#ifndef MB_BUS_H_INCLUDED
#define MB_BUS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/** Protocol identifier for BUS pattern. */
#define MB_PROTO_BUS 6
/** BUS socket types. */
/** Base MB_BUS type. */
#define MB_BUS  (MB_PROTO_BUS * 16 + 0)
/** XP variant for BUS. */
#define MB_XBUS (MB_PROTO_BUS * 16 + 8)

#ifdef __cplusplus
}
#endif

#endif
