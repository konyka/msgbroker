#ifndef MB_PAIR_H_INCLUDED
#define MB_PAIR_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif
/** MB_PROTO_PAIR is the base protocol identifier for PAIR-style sockets. */
#define MB_PROTO_PAIR 1

/** MB_PAIR base identity for PAIR protocol. */
#define MB_PAIR  (MB_PROTO_PAIR * 16 + 0)
/** @brief MB_PAIR related constants.
 *  @name MB_PAIR_CONSTANTS
 *  @{ 
 */
#define MB_XPAIR (MB_PROTO_PAIR * 16 + 8)
/** @} */

#ifdef __cplusplus
}
#endif

#endif
