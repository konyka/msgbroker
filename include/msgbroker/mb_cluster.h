#ifndef MB_CLUSTER_H_INCLUDED
#define MB_CLUSTER_H_INCLUDED

#include "mb.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Join a cluster on socket s to the given cluster_addr. */
MB_EXPORT int mb_cluster_join (int s, const char *cluster_addr);
/** Leave the current cluster on socket s. */
MB_EXPORT int mb_cluster_leave (int s);
/** Route a message to a specific key within the cluster. */
MB_EXPORT int mb_cluster_route (int s, const void *key, size_t keylen);
/** Set the replication factor for the cluster on socket s. */
MB_EXPORT int mb_cluster_replicate (int s, int replica_count);

/** Cluster configuration option names. */
#define MB_CLUSTER_NODE_ID          1
#define MB_CLUSTER_GOSSIP_INTERVAL  2
#define MB_CLUSTER_REPLICA_COUNT    3
#define MB_CLUSTER_SHARD_KEY        4

#ifdef __cplusplus
}
#endif

#endif
