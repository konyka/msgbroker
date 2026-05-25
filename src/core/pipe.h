#ifndef MB_CORE_PIPE_H_INCLUDED
#define MB_CORE_PIPE_H_INCLUDED

#include "../transport.h"

struct mb_pipe {
    struct mb_pipebase base;
    struct mb_list_item item;
};

#endif
