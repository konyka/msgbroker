/*
    msgbroker -- High-performance messaging library in pure C.

    Copyright 2024 msgbroker contributors. All rights reserved.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/

#include "limits.h"

#include <errno.h>

static int g_rcvbuf;
static int g_sndtimeo;
static int g_rcvtimeo;
static int g_backlog;
static int g_installed;

static int set_cap_int (int *slot, const void *optval, size_t optvallen)
{
    int v;

    if (!optval || optvallen != sizeof (int))
        return -EINVAL;
    v = *(const int *) optval;
    if (v < 1)
        return -EINVAL;
    *slot = v;
    return 0;
}

void mb_limits_install_defaults (void)
{
    if (g_installed)
        return;
    g_rcvbuf    = MB_LIMITS_DEFAULT_RCVBUF;
    g_sndtimeo  = MB_LIMITS_DEFAULT_TIMEO_MS;
    g_rcvtimeo  = MB_LIMITS_DEFAULT_TIMEO_MS;
    g_backlog   = MB_LIMITS_DEFAULT_BACKLOG;
    g_installed = 1;
}

int mb_limits_get_rcvbuf    (void) { return g_rcvbuf; }
int mb_limits_get_sndtimeo  (void) { return g_sndtimeo; }
int mb_limits_get_rcvtimeo  (void) { return g_rcvtimeo; }
int mb_limits_get_backlog   (void) { return g_backlog; }

int mb_limits_set_rcvbuf   (const void *optval, size_t optvallen)
{ return set_cap_int (&g_rcvbuf,   optval, optvallen); }

int mb_limits_set_sndtimeo (const void *optval, size_t optvallen)
{ return set_cap_int (&g_sndtimeo, optval, optvallen); }

int mb_limits_set_rcvtimeo (const void *optval, size_t optvallen)
{ return set_cap_int (&g_rcvtimeo, optval, optvallen); }

int mb_limits_set_backlog  (const void *optval, size_t optvallen)
{ return set_cap_int (&g_backlog,  optval, optvallen); }

/* -1 == "forever" passes any cap; otherwise the requested value must
    not exceed the active cap or -EPERM is returned. */
static int check_cap (int cap, int requested)
{
    if (requested < 0)
        return 0;
    if (requested > cap)
        return -EPERM;
    return 0;
}

int mb_limits_check_rcvbuf   (int requested) { return check_cap (g_rcvbuf,   requested); }
int mb_limits_check_sndtimeo (int requested) { return check_cap (g_sndtimeo, requested); }
int mb_limits_check_rcvtimeo (int requested) { return check_cap (g_rcvtimeo, requested); }
int mb_limits_check_backlog  (int requested) { return check_cap (g_backlog,  requested); }
