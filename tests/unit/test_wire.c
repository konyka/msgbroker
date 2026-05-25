#include "../../src/utils/wire.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main (void)
{
    uint8_t buf [8];

    mb_wire_put_uint16 (buf, 0x1234);
    assert (mb_wire_get_uint16 (buf) == 0x1234);

    mb_wire_put_uint32 (buf, 0xdeadbeef);
    assert (mb_wire_get_uint32 (buf) == 0xdeadbeef);

    mb_wire_put_uint64 (buf, 0x0102030405060708ULL);
    assert (mb_wire_get_uint64 (buf) == 0x0102030405060708ULL);

    mb_wire_put_uint64 (buf, 0xffffffffffffffffULL);
    assert (mb_wire_get_uint64 (buf) == 0xffffffffffffffffULL);

    mb_wire_put_uint16 (buf, 0);
    assert (mb_wire_get_uint16 (buf) == 0);

    printf ("test_wire: PASSED\n");
    return 0;
}
