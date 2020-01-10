#ifndef _DEBOUNCER_H_
#define _DEBOUNCER_H_

typedef struct _debouncer_t
{
	int prev_val;
	unsigned history;
	unsigned mask;
} debouncer_t;

void debouncer_init(debouncer_t *db, int hist_len, int init_val);

void debouncer_add(debouncer_t *db, int new_val);

int debouncer_value(debouncer_t *db);

void debouncer_reset(debouncer_t *db, int init_val);


#endif // _DEBOUNCER_H_
