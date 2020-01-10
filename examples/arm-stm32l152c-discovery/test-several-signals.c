#include "runtime/lib.h"
#include "kernel/uos.h"
#include "timer/timer.h"

ARRAY (task_space1, 0x400);
ARRAY (task_space2, 0x400);
ARRAY (emit_space,  0x400);

timer_t timer;
mutex_t mutex1;
mutex_t mutex2;
mutex_t mutex3;
ARRAY(mgroup_data1, sizeof(mutex_group_t) + 3 * sizeof(mutex_slot_t));
ARRAY(mgroup_data2, sizeof(mutex_group_t) + 3 * sizeof(mutex_slot_t));

void task (void *arg)
{
    void *signal;
    mutex_group_t *mgroup;
    mutex_t *pmutex;
    int task_nb = (int) arg;
    
    if (task_nb == 1)
        mgroup = mutex_group_init(mgroup_data1, sizeof(mgroup_data1));
    else
        mgroup = mutex_group_init(mgroup_data2, sizeof(mgroup_data2));
    mutex_group_add(mgroup, &mutex1);
    mutex_group_add(mgroup, &mutex2);
    mutex_group_add(mgroup, &mutex3);
    mutex_group_listen(mgroup);
    
	for (;;) {
	    mutex_group_wait(mgroup, &pmutex, &signal);
	    debug_printf("task%d: signal %d\n", task_nb, (int)signal);
	}
}

void emitter_task (void *arg)
{
    for (;;) {
        mutex_wait(&timer.lock);
        
        debug_printf("----------------\n");
        
        mutex_signal(&mutex1, (void *)1);
        mutex_signal(&mutex2, (void *)2);
        mutex_signal(&mutex3, (void *)3);
    }
}

void uos_init (void)
{
    timer_init(&timer, KHZ, 500);
    
	task_create (task, (void *)1, "task1", 1, task_space1, sizeof (task_space1));
	task_create (task, (void *)2, "task2", 2, task_space2, sizeof (task_space2));
	task_create (emitter_task, "emit", "emit", 3, emit_space, sizeof (emit_space));
}
