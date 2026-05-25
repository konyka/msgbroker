#include "../utils/err.h"
#include "../utils/alloc.h"
#include "../memory/msg.h"

#include "global.h"
#include "sock.h"

#include <msgbroker/mb.h>

int mb_device (int s1, int s2)
{
    struct mb_sock *sock1;
    struct mb_sock *sock2;
    int rc;

    rc = mb_global_hold_socket (&sock1, s1);
    if (rc < 0) return -1;

    rc = mb_global_hold_socket (&sock2, s2);
    if (rc < 0) {
        mb_global_rele_socket (sock1);
        return -1;
    }

    for (;;) {
        struct mb_msg msg;
        mb_msg_init (&msg, 0);
        rc = mb_sock_recv (sock1, &msg);
        if (rc < 0) { mb_msg_term (&msg); break; }
        rc = mb_sock_send (sock2, &msg);
        if (rc < 0) { mb_msg_term (&msg); break; }
        mb_msg_term (&msg);

        mb_msg_init (&msg, 0);
        rc = mb_sock_recv (sock2, &msg);
        if (rc < 0) { mb_msg_term (&msg); break; }
        rc = mb_sock_send (sock1, &msg);
        if (rc < 0) { mb_msg_term (&msg); break; }
        mb_msg_term (&msg);
    }

    mb_global_rele_socket (sock1);
    mb_global_rele_socket (sock2);
    return 0;
}
