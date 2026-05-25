#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <msgbroker/mb.h>

int main (void)
{
    const char *s;

    s = mb_strerror (ETERM);
    assert (s != NULL);
    assert (strlen (s) > 0);

    s = mb_strerror (EFSM);
    assert (s != NULL);
    assert (strlen (s) > 0);

    s = mb_strerror (ECONNREFUSED);
    assert (s != NULL);
    assert (strlen (s) > 0);

    s = mb_strerror (EAGAIN);
    assert (s != NULL);
    assert (strlen (s) > 0);

    s = mb_strerror (ETIMEDOUT);
    assert (s != NULL);
    assert (strlen (s) > 0);

    s = mb_strerror (EPROTONOSUPPORT);
    assert (s != NULL);
    assert (strlen (s) > 0);

    s = mb_strerror (EINVAL);
    assert (s != NULL);
    assert (strlen (s) > 0);

    printf ("test_strerror: PASSED\n");
    return 0;
}
