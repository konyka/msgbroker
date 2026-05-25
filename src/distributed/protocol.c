#include "protocol.h"
#include "../utils/wire.h"
#include <string.h>

void mb_dist_msg_init (struct mb_dist_msg *self, uint32_t type,
    uint32_t src_node_id, uint32_t dst_node_id)
{
    self->magic = MB_PROTO_DIST_MAGIC;
    self->version = MB_PROTO_DIST_VERSION;
    self->type = type;
    self->src_node_id = src_node_id;
    self->dst_node_id = dst_node_id;
    self->payload_len = 0;
}

void mb_dist_msg_hton (struct mb_dist_msg *self)
{
    uint8_t buf[24];
    mb_wire_put_uint32 (buf, self->magic);
    mb_wire_put_uint32 (buf + 4, self->version);
    mb_wire_put_uint32 (buf + 8, self->type);
    mb_wire_put_uint32 (buf + 12, self->src_node_id);
    mb_wire_put_uint32 (buf + 16, self->dst_node_id);
    mb_wire_put_uint32 (buf + 20, self->payload_len);
    memcpy (self, buf, 24);
}

void mb_dist_msg_ntoh (struct mb_dist_msg *self)
{
    uint8_t buf[24];
    memcpy (buf, self, 24);
    self->magic = mb_wire_get_uint32 (buf);
    self->version = mb_wire_get_uint32 (buf + 4);
    self->type = mb_wire_get_uint32 (buf + 8);
    self->src_node_id = mb_wire_get_uint32 (buf + 12);
    self->dst_node_id = mb_wire_get_uint32 (buf + 16);
    self->payload_len = mb_wire_get_uint32 (buf + 20);
}

size_t mb_dist_msg_header_size (void)
{
    return sizeof (struct mb_dist_msg);
}
