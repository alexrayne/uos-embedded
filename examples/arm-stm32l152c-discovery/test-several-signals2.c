#include "runtime/lib.h"
#include "kernel/uos.h"
#include "timer/timer.h"

ARRAY (task_space1, 0x400);
ARRAY (task_space2, 0x400);
ARRAY (emit_space,  0x400);

timer_t timer;
mutex_t mutex;

void task (void *arg)
{
    int signal;
    int task_nb = (int) arg;
    
    mutex_lock(&mutex);
    
	for (;;) {
	    signal = (int)mutex_wait(&mutex);
	    debug_printf("task%d: signal %d\n", task_nb, signal);
	}
}

void emitter_task (void *arg)
{
    for (;;) {
        mutex_wait(&timer.lock);
        
        debug_printf("----------------\n");
        
        mutex_signal(&mutex, (void *)1);
        mutex_signal(&mutex, (void *)2);
        mutex_signal(&mutex, (void *)3);
    }
}

void uos_init (void)
{
    timer_init(&timer, KHZ, 500);
    
	task_create (task, (void *)1, "task1", 1, task_space1, sizeof (task_space1));
	task_create (task, (void *)2, "task2", 2, task_space2, sizeof (task_space2));
	task_create (emitter_task, "emit", "emit", 3, emit_space, sizeof (emit_space));
}
