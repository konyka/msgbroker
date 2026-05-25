#ifndef MB_CLUSTER_H_INCLUDED
#define MB_CLUSTER_H_INCLUDED

#include "mb.h"

#ifdef __cplusplus
extern "C" {
#endif

MB_EXPORT int mb_cluster_join (int s, const char *cluster_addr);
MB_EXPORT int mb_cluster_leave (int s);
MB_EXPORT int mb_cluster_route (int s, const void *key, size_t keylen);
MB_EXPORT int mb_cluster_replicate (int s, int replica_count);

#define MB_CLUSTER_NODE_ID          1
#define MB_CLUSTER_GOSSIP_INTERVAL  2
#define MB_CLUSTER_REPLICA_COUNT    3
#define MB_CLUSTER_SHARD_KEY        4

#ifdef __cplusplus
}
#endif

#endif
