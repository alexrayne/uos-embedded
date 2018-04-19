#include "int_debouncer.h"

void int_debouncer_init(int_debouncer_t *db, int init_val)
{
	db->value = init_val;
	
	int i;
	for (i = 0; i < INT_DEBOUNCER_HIST_DEPTH; ++i)
		db->history[i] = init_val;
}

void int_debouncer_add(int_debouncer_t *db, int new_val)
{
	int i;
	int change_val = 1;

	for (i = 0; i < INT_DEBOUNCER_HIST_DEPTH - 1; ++i)
		db->history[i] = db->history[i+1];
		
	db->history[i] = new_val;
	
	for (i = 0; i < INT_DEBOUNCER_HIST_DEPTH - 1; ++i)
		if (db->history[i] != new_val)
			change_val = 0;
	
	if (change_val)
		db->value = new_val;
}

int int_debouncer_value(int_debouncer_t *db)
{
	return db->value;
}

void int_debouncer_reset(int_debouncer_t *db, int init_val)
{
    int_debouncer_init(db, init_val);
}
