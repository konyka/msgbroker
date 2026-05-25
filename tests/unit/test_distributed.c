#include "../../src/distributed/ring.h"
#include "../../src/distributed/gossip.h"
#include "../../src/distributed/protocol.h"
#include "../../src/pal/clock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

static void test_ring_basic (void)
{
    struct mb_ring ring;
    mb_ring_init (&ring, 10);

    int rc = mb_ring_add (&ring, 1, "node1:9000");
    assert (rc == 0);
    assert (mb_ring_count (&ring) == 1);

    rc = mb_ring_add (&ring, 2, "node2:9000");
    assert (rc == 0);
    assert (mb_ring_count (&ring) == 2);

    mb_ring_term (&ring);
    printf ("  test_ring_basic: PASSED\n");
}

static void test_ring_lookup (void)
{
    struct mb_ring ring;
    mb_ring_init (&ring, 150);

    mb_ring_add (&ring, 1, "node1:9000");
    mb_ring_add (&ring, 2, "node2:9000");
    mb_ring_add (&ring, 3, "node3:9000");

    uint32_t n1 = mb_ring_lookup (&ring, "key1", 4);
    uint32_t n2 = mb_ring_lookup (&ring, "key2", 4);
    assert (n1 != 0);
    assert (n2 != 0);

    uint32_t n1_again = mb_ring_lookup (&ring, "key1", 4);
    assert (n1 == n1_again);

    mb_ring_term (&ring);
    printf ("  test_ring_lookup: PASSED\n");
}

static void test_ring_remove (void)
{
    struct mb_ring ring;
    mb_ring_init (&ring, 10);

    mb_ring_add (&ring, 1, "node1:9000");
    mb_ring_add (&ring, 2, "node2:9000");

    mb_ring_remove (&ring, 1);
    assert (mb_ring_count (&ring) == 1);

    mb_ring_term (&ring);
    printf ("  test_ring_remove: PASSED\n");
}

static void test_gossip_basic (void)
{
    struct mb_gossip_config cfg;
    cfg.local_node_id = 1;
    strncpy (cfg.local_addr, "127.0.0.1:9000", MB_GOSSIP_ADDR_LEN - 1);
    cfg.interval_ms = 100;
    cfg.suspect_timeout_ms = 300;
    cfg.dead_timeout_ms = 1000;

    struct mb_gossip gossip;
    mb_gossip_init (&gossip, &cfg);

    assert (mb_gossip_node_count (&gossip) == 1);

    int rc = mb_gossip_add_node (&gossip, 2, "127.0.0.1:9001");
    assert (rc == 0);
    assert (mb_gossip_node_count (&gossip) == 2);

    rc = mb_gossip_add_node (&gossip, 2, "127.0.0.1:9001");
    assert (rc == -EEXIST);

    struct mb_gossip_node *node = mb_gossip_find_node (&gossip, 2);
    assert (node != NULL);
    assert (node->state == MB_GOSSIP_NODE_ALIVE);

    mb_gossip_remove_node (&gossip, 2);
    assert (mb_gossip_node_count (&gossip) == 1);

    mb_gossip_term (&gossip);
    printf ("  test_gossip_basic: PASSED\n");
}

static int g_gossip_changes = 0;

static void test_gossip_callback (void *ctx, struct mb_gossip_node *node,
    enum mb_gossip_node_state old_state)
{
    (void) ctx; (void) node; (void) old_state;
    g_gossip_changes++;
}

static void test_gossip_tick (void)
{
    struct mb_gossip_config cfg;
    cfg.local_node_id = 1;
    strncpy (cfg.local_addr, "127.0.0.1:9000", MB_GOSSIP_ADDR_LEN - 1);
    cfg.interval_ms = 100;
    cfg.suspect_timeout_ms = 50;
    cfg.dead_timeout_ms = 200;

    struct mb_gossip gossip;
    mb_gossip_init (&gossip, &cfg);
    mb_gossip_set_callback (&gossip, test_gossip_callback, NULL);

    mb_gossip_add_node (&gossip, 2, "127.0.0.1:9001");

    struct mb_gossip_node *node = mb_gossip_find_node (&gossip, 2);
    assert (node);

    node->last_seen_ms = mb_clock_ms () - 100;

    g_gossip_changes = 0;
    mb_gossip_tick (&gossip);
    assert (g_gossip_changes == 1);
    assert (node->state == MB_GOSSIP_NODE_SUSPECT);

    node->last_seen_ms = mb_clock_ms () - 300;
    mb_gossip_tick (&gossip);
    assert (node->state == MB_GOSSIP_NODE_DEAD);

    mb_gossip_term (&gossip);
    printf ("  test_gossip_tick: PASSED\n");
}

static void test_protocol_serialization (void)
{
    struct mb_dist_msg msg;
    mb_dist_msg_init (&msg, MB_PROTO_DIST_PING, 1, 2);

    assert (msg.magic == MB_PROTO_DIST_MAGIC);
    assert (msg.type == MB_PROTO_DIST_PING);
    assert (msg.src_node_id == 1);
    assert (msg.dst_node_id == 2);

    struct mb_dist_msg copy = msg;
    mb_dist_msg_hton (&copy);
    mb_dist_msg_ntoh (&copy);

    assert (copy.magic == MB_PROTO_DIST_MAGIC);
    assert (copy.type == MB_PROTO_DIST_PING);
    assert (copy.src_node_id == 1);
    assert (copy.dst_node_id == 2);

    printf ("  test_protocol_serialization: PASSED\n");
}

int main (void)
{
    printf ("Distributed layer tests:\n");
    test_ring_basic ();
    test_ring_lookup ();
    test_ring_remove ();
    test_gossip_basic ();
    test_gossip_tick ();
    test_protocol_serialization ();
    printf ("All distributed tests passed.\n");
    return 0;
}
