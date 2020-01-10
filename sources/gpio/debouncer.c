#include "debouncer.h"

void debouncer_reset(debouncer_t *db, int init_val)
{
    db->prev_val = init_val;
	if (init_val)
		db->history = 0xFFFFFFFF;
	else db->history = 0x0;
}

void debouncer_init(debouncer_t *db, int hist_len, int init_val)
{
	db->mask = (1 << hist_len) - 1;
	debouncer_reset(db, init_val);
}

void debouncer_add(debouncer_t *db, int new_val)
{
	db->history = (db->history << 1) | ((new_val) ? 1 : 0);
}

int debouncer_value(debouncer_t *db)
{
	if ((db->history & db->mask) == db->mask) {
		db->prev_val = 1;
	} else if ((db->history & db->mask) == 0) {
		db->prev_val = 0;
	}
	return db->prev_val;
}

