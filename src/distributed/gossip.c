#include "gossip.h"
#include "../pal/clock.h"
#include "../pal/sleep.h"
#include "../utils/alloc.h"
#include "../utils/err.h"
#include "../utils/cont.h"

#include <string.h>
#include <errno.h>
#include <unistd.h>

static void mb_gossip_node_init (struct mb_gossip_node *node,
    uint32_t node_id, const char *addr)
{
    node->node_id = node_id;
    strncpy (node->addr, addr, MB_GOSSIP_ADDR_LEN - 1);
    node->addr[MB_GOSSIP_ADDR_LEN - 1] = '\0';
    node->incarnation = 0;
    node->state = MB_GOSSIP_NODE_ALIVE;
    node->last_seen_ms = mb_clock_ms ();
    mb_list_item_init (&node->item);
}

static void mb_gossip_node_term (struct mb_gossip_node *node)
{
    mb_list_item_term (&node->item);
}

void mb_gossip_init (struct mb_gossip *self,
    const struct mb_gossip_config *config)
{
    memcpy (&self->config, config, sizeof (self->config));
    mb_list_init (&self->nodes);
    mb_mutex_init (&self->sync);
    mb_thread_init (&self->thread);
    mb_atomic_store (&self->running, 0);
    self->on_change = NULL;
    self->on_change_ctx = NULL;

    mb_gossip_add_node (self, config->local_node_id, config->local_addr);
}

void mb_gossip_term (struct mb_gossip *self)
{
    mb_gossip_stop (self);

    struct mb_list_item *it = mb_list_begin (&self->nodes);
    while (it != mb_list_end (&self->nodes)) {
        struct mb_gossip_node *node = mb_cont (it, struct mb_gossip_node, item);
        struct mb_list_item *next = mb_list_next (&self->nodes, it);
        mb_list_erase (&self->nodes, &node->item);
        mb_free (node);
        it = next;
    }

    mb_mutex_term (&self->sync);
    mb_list_term (&self->nodes);
}

static void mb_gossip_thread_routine (void *arg)
{
    struct mb_gossip *self = (struct mb_gossip *) arg;
    while (mb_atomic_load (&self->running)) {
        mb_gossip_tick (self);
        usleep ((useconds_t) self->config.interval_ms * 1000);
    }
}

int mb_gossip_start (struct mb_gossip *self)
{
    mb_atomic_store (&self->running, 1);
    return mb_thread_start (&self->thread, mb_gossip_thread_routine, self);
}

void mb_gossip_stop (struct mb_gossip *self)
{
    if (mb_atomic_load (&self->running)) {
        mb_atomic_store (&self->running, 0);
        mb_thread_join (&self->thread);
    }
}

int mb_gossip_add_node (struct mb_gossip *self, uint32_t node_id,
    const char *addr)
{
    struct mb_gossip_node *existing = mb_gossip_find_node (self, node_id);
    if (existing)
        return -EEXIST;

    struct mb_gossip_node *node = (struct mb_gossip_node *)
        mb_alloc (sizeof (struct mb_gossip_node));
    if (!node)
        return -ENOMEM;

    mb_gossip_node_init (node, node_id, addr);

    mb_mutex_lock (&self->sync);
    mb_list_insert (&self->nodes, &node->item, mb_list_end (&self->nodes));
    mb_mutex_unlock (&self->sync);

    return 0;
}

int mb_gossip_remove_node (struct mb_gossip *self, uint32_t node_id)
{
    mb_mutex_lock (&self->sync);

    struct mb_list_item *it;
    for (it = mb_list_begin (&self->nodes);
         it != mb_list_end (&self->nodes);
         it = mb_list_next (&self->nodes, it)) {
        struct mb_gossip_node *node = mb_cont (it, struct mb_gossip_node, item);
        if (node->node_id == node_id) {
            mb_list_erase (&self->nodes, &node->item);
            mb_gossip_node_term (node);
            mb_free (node);
            mb_mutex_unlock (&self->sync);
            return 0;
        }
    }

    mb_mutex_unlock (&self->sync);
    return -ENOENT;
}

struct mb_gossip_node *mb_gossip_find_node (struct mb_gossip *self,
    uint32_t node_id)
{
    struct mb_list_item *it;
    for (it = mb_list_begin (&self->nodes);
         it != mb_list_end (&self->nodes);
         it = mb_list_next (&self->nodes, it)) {
        struct mb_gossip_node *node = mb_cont (it, struct mb_gossip_node, item);
        if (node->node_id == node_id)
            return node;
    }
    return NULL;
}

int mb_gossip_node_count (struct mb_gossip *self)
{
    int count = 0;
    struct mb_list_item *it;
    for (it = mb_list_begin (&self->nodes);
         it != mb_list_end (&self->nodes);
         it = mb_list_next (&self->nodes, it)) {
        count++;
    }
    return count;
}

void mb_gossip_set_callback (struct mb_gossip *self,
    mb_gossip_on_change cb, void *ctx)
{
    self->on_change = cb;
    self->on_change_ctx = ctx;
}

void mb_gossip_tick (struct mb_gossip *self)
{
    uint64_t now = mb_clock_ms ();
    int suspect_ms = self->config.suspect_timeout_ms > 0 ?
        self->config.suspect_timeout_ms : MB_GOSSIP_DEFAULT_INTERVAL_MS * 3;
    int dead_ms = self->config.dead_timeout_ms > 0 ?
        self->config.dead_timeout_ms : MB_GOSSIP_DEFAULT_INTERVAL_MS * 10;

    mb_mutex_lock (&self->sync);

    struct mb_list_item *it;
    for (it = mb_list_begin (&self->nodes);
         it != mb_list_end (&self->nodes);
         it = mb_list_next (&self->nodes, it)) {
        struct mb_gossip_node *node = mb_cont (it, struct mb_gossip_node, item);
        if (node->node_id == self->config.local_node_id)
            continue;

        uint64_t elapsed = now - node->last_seen_ms;
        enum mb_gossip_node_state old_state = node->state;

        if (node->state == MB_GOSSIP_NODE_ALIVE &&
            elapsed > (uint64_t) suspect_ms) {
            node->state = MB_GOSSIP_NODE_SUSPECT;
        }
        if (node->state == MB_GOSSIP_NODE_SUSPECT &&
            elapsed > (uint64_t) dead_ms) {
            node->state = MB_GOSSIP_NODE_DEAD;
        }

        if (old_state != node->state && self->on_change) {
            self->on_change (self->on_change_ctx, node, old_state);
        }
    }

    mb_mutex_unlock (&self->sync);
}
