#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <msgbroker/mb.h>
#include <msgbroker/mb_pair.h>

int main (void)
{
    int s = mb_socket (AF_MB, MB_PAIR);
    assert (s >= 0);

    uint64_t stat = mb_get_statistic (s, MB_STAT_MESSAGES_SENT);
    assert (stat == 0);

    stat = mb_get_statistic (s, MB_STAT_BYTES_SENT);
    assert (stat == 0);

    stat = mb_get_statistic (s, MB_STAT_ESTABLISHED_CONNECTIONS);
    assert (stat == 0);

    int rc = mb_close (s);
    assert (rc == 0);

    printf ("test_pipe: PASSED\n");
    return 0;
}
