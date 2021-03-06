#ifndef __LIST_H_
#define __LIST_H_ 1

#ifndef INLINE
#include <uos-conf.h>
#ifndef INLINE
#   ifdef __cplusplus
#       define INLINE inline
#   else
#       define INLINE static inline
#   endif
#endif
#endif

// depends by bool_t
#include <runtime/arch.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Extra fast implementation of doubly linked list, by LY.
 * Some ideas coming from Linux's source code.
 */
typedef struct _list_t {
	struct _list_t *next;
	struct _list_t *prev;
} list_t;

#ifdef __cplusplus
}
#endif


/*
 * Initialize an empty list, or an unlinked element.
 * Both pointers are linked to the list header itself.
 */
INLINE void list_init (list_t *l)
{
	l->next = l;
	l->prev = l;
}

/*
 * Insert a new element between the two neigbour elements 'prev' and 'next'.
 * Internal function.
 */
INLINE void __list_put_in_between (list_t *elem, list_t *left, list_t *right)
{
	right->prev = elem;
	elem->next = right;
	elem->prev = left;
	left->next = elem;
}

/*
 * Connect two elements together, thus removing all in between.
 * Internal function.
 */
INLINE void __list_connect_together (list_t *left, list_t *right)
{
	right->prev = left;
	left->next = right;
}

/*
 * Insert an element at the begginning of the list.
 */
INLINE void list_prepend (list_t *l, list_t *elem)
{
	__list_connect_together (elem->prev, elem->next);
	__list_put_in_between (elem, l, l->next);
}

/*
 * Insert an element at the end of the list.
 */
INLINE void list_append (list_t *l, list_t *elem)
{
	__list_connect_together (elem->prev, elem->next);
	__list_put_in_between (elem, l->prev, l);
}

/*
 * Remove an element from any list.
 */
INLINE void list_unlink (list_t *elem)
{
	__list_connect_together (elem->prev, elem->next);
	list_init (elem);
}

/*
 * Check that list is empty.
 */
INLINE bool_t list_is_empty (const list_t *l)
{
	return l->next == l;
}

/*
 * Get the first list item.
 */
INLINE list_t *list_first (const list_t *l)
{
	return l->next;
}

#define list_iterate_from(i, first, head)       for (i = (typeof(i)) first; \
                         i != (typeof(i)) (head); \
                         i = (typeof(i)) ((list_t*) i)->next)


/*
 * Iterate through all list items, from first to last.
 * Example:
 *	struct my_list_type *i;
 *	list_iterate (i, my_list) {
 *		process_one_element (i);
 *	}
 */
#define list_iterate(i, head)		for (i = (typeof(i)) (head)->next; \
					     i != (typeof(i)) (head); \
					     i = (typeof(i)) ((list_t*) i)->next)

/*
 * same as list_iterate, but preserves lookahed list item, so it possible to 
 *  move/remove current list item safely, no breaking iterating process  
 * Example:
 *  struct my_list_type *i;
 *  struct my_list_type *next;
 *  list_safe_iterate (i, next, my_list) {
 *      process_one_element (i);
 *  }
 */
#define list_safe_iterate(i, next_i, head)  for (i = (typeof(i)) (head)->next, next_i = (typeof(next_i)) (((list_t*) i)->next);\
                         i != (typeof(i)) (head); \
                         i = (typeof(i))next_i, next_i = (typeof(next_i))(((list_t*)next_i)->next) )

/*
 * Iterate through all list items, from last to first.
 * Example:
 *	list_t *i;
 *	list_iterate_backward (i, my_list) {
 *		process_one_element (i);
 *	}
 */
#define list_iterate_backward(i, head)	for (i = (typeof(i)) (head)->prev; \
					     i != (typeof(i)) (head); \
					     i = (typeof(i)) ((list_t*) i)->prev)

INLINE
bool_t list_contains (const list_t *l, const list_t *x)
{
    const list_t *i;
    list_iterate(i, l)
        if (i == x)
            return 1;
    return 0;
}

#endif /* __LIST_H_ */
