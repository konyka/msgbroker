#include "list.h"
#include "fast.h"

#include <stddef.h>

void mb_list_init (struct mb_list *self)
{
    self->first = NULL;
    self->last = NULL;
}

void mb_list_term (struct mb_list *self)
{
    self->first = NULL;
    self->last = NULL;
}

int mb_list_empty (struct mb_list *self)
{
    return self->first == NULL;
}

struct mb_list_item *mb_list_begin (struct mb_list *self)
{
    return self->first;
}

struct mb_list_item *mb_list_end (struct mb_list *self)
{
    (void) self;
    return NULL;
}

struct mb_list_item *mb_list_prev (struct mb_list *self,
    struct mb_list_item *it)
{
    (void) self;
    return it ? it->prev : NULL;
}

struct mb_list_item *mb_list_next (struct mb_list *self,
    struct mb_list_item *it)
{
    (void) self;
    return it ? it->next : NULL;
}

void mb_list_insert (struct mb_list *self, struct mb_list_item *item,
    struct mb_list_item *before)
{
    if (!before) {
        item->prev = self->last;
        item->next = NULL;
        if (self->last)
            self->last->next = item;
        else
            self->first = item;
        self->last = item;
    } else {
        item->prev = before->prev;
        item->next = before;
        if (before->prev)
            before->prev->next = item;
        else
            self->first = item;
        before->prev = item;
    }
}

struct mb_list_item *mb_list_erase (struct mb_list *self,
    struct mb_list_item *item)
{
    if (item->prev)
        item->prev->next = item->next;
    else
        self->first = item->next;
    if (item->next)
        item->next->prev = item->prev;
    else
        self->last = item->prev;
    item->prev = MB_LIST_NOTINLIST;
    item->next = MB_LIST_NOTINLIST;
    return item->next;
}

void mb_list_item_init (struct mb_list_item *self)
{
    self->prev = MB_LIST_NOTINLIST;
    self->next = MB_LIST_NOTINLIST;
}

void mb_list_item_term (struct mb_list_item *self)
{
    self->prev = MB_LIST_NOTINLIST;
    self->next = MB_LIST_NOTINLIST;
}

int mb_list_item_isinlist (struct mb_list_item *self)
{
    return self->prev != MB_LIST_NOTINLIST;
}
