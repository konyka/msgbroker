#ifndef MB_LIST_H_INCLUDED
#define MB_LIST_H_INCLUDED

struct mb_list_item {
    struct mb_list_item *next;
    struct mb_list_item *prev;
};

struct mb_list {
    struct mb_list_item *first;
    struct mb_list_item *last;
};

#define MB_LIST_NOTINLIST ((struct mb_list_item *) -1)
#define MB_LIST_ITEM_INITIALIZER {MB_LIST_NOTINLIST, MB_LIST_NOTINLIST}

void mb_list_init (struct mb_list *self);
void mb_list_term (struct mb_list *self);
int mb_list_empty (struct mb_list *self);
struct mb_list_item *mb_list_begin (struct mb_list *self);
struct mb_list_item *mb_list_end (struct mb_list *self);
struct mb_list_item *mb_list_prev (struct mb_list *self, struct mb_list_item *it);
struct mb_list_item *mb_list_next (struct mb_list *self, struct mb_list_item *it);
void mb_list_insert (struct mb_list *self, struct mb_list_item *item,
    struct mb_list_item *before);
struct mb_list_item *mb_list_erase (struct mb_list *self,
    struct mb_list_item *item);
void mb_list_item_init (struct mb_list_item *self);
void mb_list_item_term (struct mb_list_item *self);
int mb_list_item_isinlist (struct mb_list_item *self);

#endif
