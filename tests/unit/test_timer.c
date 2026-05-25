#include "../../src/aio/timer.h"
#include "../../src/aio/fsm.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main (void)
{
    struct mb_timer timer;
    struct mb_fsm owner;
    mb_fsm_init_root (&owner, NULL, NULL, NULL);
    mb_timer_init (&timer, 1, &owner);

    assert (mb_fsm_isidle (&timer.fsm));

    mb_timer_term (&timer);
    mb_fsm_term (&owner);
    printf ("test_timer: PASSED\n");
    return 0;
}
