#ifndef MB_ERR_H_INCLUDED
#define MB_ERR_H_INCLUDED

#include <stddef.h>

int mb_err_errno (void);
void mb_err_set_errno (int err);
const char *mb_err_strerror (int errnum);

#endif
