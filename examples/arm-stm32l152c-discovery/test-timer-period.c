/*
 * Проверка работы функции смены периода таймера.
 */
#include <runtime/lib.h>
#include <kernel/uos.h>
#include <timer/timer.h>

ARRAY (task, 1000);
timer_t timer;

void hello (void *arg)
{
	for (;;) {
		debug_printf ("%s: msec = %d\n",
			arg, timer_milliseconds (&timer));
	    if (timer_passed (&timer, 0, 5000))
	        timer_set_period (&timer, 1000);
		mutex_wait (&timer.lock);
	}
}

void uos_init (void)
{
	debug_printf ("\nTesting timer.\n");
#ifdef NSEC_TIMER
    timer_init_ns (&timer, KHZ, 100000000);
#else
	timer_init (&timer, KHZ, 100);
#endif
	task_create (hello, "Timer", "hello", 1, task, sizeof (task));
}

