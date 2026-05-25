#include "discovery.h"

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MB_DISCOVERY_PKT_MAGIC 0x4D424453
#define MB_DISCOVERY_PKT_VERSION 1

struct mb_discovery_pkt {
    uint32_t magic;
    uint32_t version;
    uint32_t node_id;
    char addr[MB_DISCOVERY_MAX_ADDR];
};

void mb_discovery_init (struct mb_discovery *self,
    const struct mb_discovery_config *config)
{
    memcpy (&self->config, config, sizeof (self->config));
    mb_thread_init (&self->thread);
    mb_atomic_store (&self->running, 0);
    self->sock_fd = -1;
    self->on_node = NULL;
    self->on_node_ctx = NULL;
}

void mb_discovery_term (struct mb_discovery *self)
{
    mb_discovery_stop (self);
    mb_thread_term (&self->thread);
}

static void mb_discovery_broadcast (struct mb_discovery *self)
{
    struct mb_discovery_pkt pkt;
    pkt.magic = MB_DISCOVERY_PKT_MAGIC;
    pkt.version = MB_DISCOVERY_PKT_VERSION;
    pkt.node_id = self->config.local_node_id;
    strncpy (pkt.addr, self->config.local_addr, MB_DISCOVERY_MAX_ADDR - 1);
    pkt.addr[MB_DISCOVERY_MAX_ADDR - 1] = '\0';

    struct sockaddr_in addr;
    memset (&addr, 0, sizeof (addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons ((uint16_t) self->config.port);
    inet_pton (AF_INET, self->config.multicast_group, &addr.sin_addr);

    sendto (self->sock_fd, &pkt, sizeof (pkt), 0,
        (struct sockaddr *) &addr, sizeof (addr));
}

static void mb_discovery_listen (struct mb_discovery *self)
{
    struct sockaddr_in addr;
    memset (&addr, 0, sizeof (addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons ((uint16_t) self->config.port);
    addr.sin_addr.s_addr = htonl (INADDR_ANY);

    if (bind (self->sock_fd, (struct sockaddr *) &addr, sizeof (addr)) < 0)
        return;

    struct ip_mreq mreq;
    memset (&mreq, 0, sizeof (mreq));
    inet_pton (AF_INET, self->config.multicast_group, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = htonl (INADDR_ANY);
    setsockopt (self->sock_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
        &mreq, sizeof (mreq));
}

static void mb_discovery_thread_routine (void *arg)
{
    struct mb_discovery *self = (struct mb_discovery *) arg;

    mb_discovery_listen (self);

    while (mb_atomic_load (&self->running)) {
        mb_discovery_broadcast (self);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        fd_set fds;
        FD_ZERO (&fds);
        FD_SET (self->sock_fd, &fds);

        while (select (self->sock_fd + 1, &fds, NULL, NULL, &tv) > 0) {
            struct mb_discovery_pkt pkt;
            struct sockaddr_in from;
            socklen_t fromlen = sizeof (from);
            ssize_t n = recvfrom (self->sock_fd, &pkt, sizeof (pkt), 0,
                (struct sockaddr *) &from, &fromlen);
            if (n == (ssize_t) sizeof (pkt) &&
                pkt.magic == MB_DISCOVERY_PKT_MAGIC &&
                pkt.version == MB_DISCOVERY_PKT_VERSION &&
                pkt.node_id != self->config.local_node_id) {
                if (self->on_node)
                    self->on_node (self->on_node_ctx, pkt.node_id, pkt.addr);
            }
            FD_ZERO (&fds);
            FD_SET (self->sock_fd, &fds);
            tv.tv_sec = 0;
            tv.tv_usec = 0;
        }

        usleep ((useconds_t) self->config.interval_ms * 1000);
    }
}

int mb_discovery_start (struct mb_discovery *self)
{
    self->sock_fd = socket (AF_INET, SOCK_DGRAM, 0);
    if (self->sock_fd < 0)
        return -1;

    int reuse = 1;
    setsockopt (self->sock_fd, SOL_SOCKET, SO_REUSEADDR,
        &reuse, sizeof (reuse));

    mb_atomic_store (&self->running, 1);
    return mb_thread_start (&self->thread, mb_discovery_thread_routine, self);
}

void mb_discovery_stop (struct mb_discovery *self)
{
    if (mb_atomic_load (&self->running)) {
        mb_atomic_store (&self->running, 0);
        mb_thread_join (&self->thread);
    }
    if (self->sock_fd >= 0) {
        close (self->sock_fd);
        self->sock_fd = -1;
    }
}
