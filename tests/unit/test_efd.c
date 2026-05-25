#include "../../src/pal/efd.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main (void)
{
    struct mb_efd efd;

    mb_efd_init (&efd);
    assert (mb_efd_getfd (&efd) >= 0);

    mb_efd_signal (&efd);

    int rc = mb_efd_wait (&efd, 100);
    assert (rc == 0);
    (void) rc;

    mb_efd_unsignal (&efd);

    rc = mb_efd_wait (&efd, 50);
    assert (rc != 0);
    (void) rc;

    printf ("test_efd: PASSED\n");

    mb_efd_term (&efd);
    return 0;
}
