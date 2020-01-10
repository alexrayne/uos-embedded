#ifndef _INT_DEBOUNCER_H_
#define _INT_DEBOUNCER_H_

// Значение INT_DEBOUNCER_HIST_DEPTH не должно быть меньше 2!
#ifndef INT_DEBOUNCER_HIST_DEPTH
#define INT_DEBOUNCER_HIST_DEPTH	3
#endif

typedef struct _int_debouncer_t
{
	int value;
	int history[INT_DEBOUNCER_HIST_DEPTH];
} int_debouncer_t;

void int_debouncer_init(int_debouncer_t *db, int init_val);

void int_debouncer_add(int_debouncer_t *db, int new_val);

int int_debouncer_value(int_debouncer_t *db);

void int_debouncer_reset(int_debouncer_t *db, int init_val);


#endif // _INT_DEBOUNCER_H_
