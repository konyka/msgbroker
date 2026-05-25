#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>
#include <msgbroker/mb_cluster.h>
#include <stdio.h>
#include <string.h>

int main (void)
{
    int s = mb_socket (AF_MB, MB_PAIR);

    int rc = mb_cluster_join (s, "127.0.0.1:9000");
    printf ("cluster join: %d\n", rc);

    if (rc == 0) {
        const char *key = "user:12345";
        int node = mb_cluster_route (s, key, strlen (key));
        printf ("route '%s' -> node %d\n", key, node);

        mb_cluster_leave (s);
        printf ("cluster leave\n");
    }

    mb_close (s);
    return 0;
}
