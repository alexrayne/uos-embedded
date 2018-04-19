/*
 * Проверка драйвера таймера.
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
		mutex_wait (&timer.decisec);
	}
}

void uos_init (void)
{
	debug_printf ("\nTesting timer.\n");
#ifdef NSEC_TIMER
    timer_init_ns (&timer, KHZ, 10000000);
#else
	timer_init (&timer, KHZ, 10);
#endif
	task_create (hello, "Timer", "hello", 1, task, sizeof (task));
}

