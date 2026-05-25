#include "../../src/aio/fsm.h"
#include "../../src/aio/ctx.h"
#include "../../src/aio/pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static int fsm_state_log [8];
static int fsm_log_idx = 0;

static void test_fsm_handler (struct mb_fsm *self, int src, int type,
    void *srcptr)
{
    (void) srcptr;
    int *state = &self->state;

    switch (*state) {
    case 0:
        if (src == MB_FSM_ACTION && type == MB_FSM_START) {
            *state = 1;
            fsm_state_log[fsm_log_idx++] = 1;
        }
        break;
    case 1:
        if (src == MB_FSM_ACTION && type == MB_FSM_STOP) {
            *state = 0;
            fsm_state_log[fsm_log_idx++] = 0;
        } else if (src == 1 && type == 42) {
            *state = 2;
            fsm_state_log[fsm_log_idx++] = 2;
        }
        break;
    case 2:
        if (src == MB_FSM_ACTION && type == MB_FSM_STOP) {
            *state = 0;
            fsm_state_log[fsm_log_idx++] = 0;
        }
        break;
    }
}

int main (void)
{
    struct mb_fsm fsm;
    mb_fsm_init_root (&fsm, test_fsm_handler, NULL, NULL);

    assert (mb_fsm_isidle (&fsm));
    mb_fsm_start (&fsm);
    assert (!mb_fsm_isidle (&fsm));
    assert (fsm_state_log[0] == 1);

    fsm.fn (&fsm, 1, 42, NULL);
    assert (fsm.state == 2);
    assert (fsm_state_log[1] == 2);

    mb_fsm_stop (&fsm);
    assert (fsm.state == 0);
    assert (fsm_state_log[2] == 0);

    mb_fsm_term (&fsm);
    printf ("test_fsm: PASSED\n");
    return 0;
}
