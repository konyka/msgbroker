#include "../../src/pal/semaphore.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main (void)
{
    struct mb_sem sem;

    mb_sem_init (&sem);

    mb_sem_post (&sem);
    mb_sem_post (&sem);

    int rc = mb_sem_wait (&sem);
    assert (rc == 0);
    (void) rc;

    rc = mb_sem_wait (&sem);
    assert (rc == 0);
    (void) rc;

    rc = mb_sem_timedwait (&sem, 50);
    assert (rc != 0);
    (void) rc;

    printf ("test_semaphore: PASSED\n");

    mb_sem_term (&sem);
    return 0;
}
