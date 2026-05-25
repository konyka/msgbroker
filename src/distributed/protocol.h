#ifndef MB_DISTRIBUTED_PROTOCOL_H_INCLUDED
#define MB_DISTRIBUTED_PROTOCOL_H_INCLUDED

#include <stdint.h>
#include <stddef.h>

#define MB_PROTO_DIST_MAGIC     0x4D424452
#define MB_PROTO_DIST_VERSION   1

#define MB_PROTO_DIST_HELLO     1
#define MB_PROTO_DIST_WELCOME   2
#define MB_PROTO_DIST_PING      3
#define MB_PROTO_DIST_PONG      4
#define MB_PROTO_DIST_MEMBERS   5
#define MB_PROTO_DIST_ROUTE     6
#define MB_PROTO_DIST_DATA      7

struct mb_dist_msg {
    uint32_t magic;
    uint32_t version;
    uint32_t type;
    uint32_t src_node_id;
    uint32_t dst_node_id;
    uint32_t payload_len;
};

void mb_dist_msg_init (struct mb_dist_msg *self, uint32_t type,
    uint32_t src_node_id, uint32_t dst_node_id);
void mb_dist_msg_hton (struct mb_dist_msg *self);
void mb_dist_msg_ntoh (struct mb_dist_msg *self);

size_t mb_dist_msg_header_size (void);

#endif
