/*
 * Testing GPIO output.
 */
#include <runtime/lib.h>
#include <kernel/uos.h>
#include <timer/timer.h>
#include <stm32l1/gpio.h>

ARRAY (task_space, 0x400);	/* Memory for task stack */

timer_t timer;
stm32l1_gpio_t outpin;

void task (void *arg)
{
    int val = 0;
	for (;;) {
	    mutex_wait(&timer.lock);
		gpio_set_val(&outpin.gpioif, val);
		val = !val;
	}
}

void uos_init (void)
{
    timer_init_ns(&timer, KHZ, 1000000);
    stm32l1_gpio_init(&outpin, GPIO_PORT_A, 1, GPIO_FLAGS_OUTPUT);
	task_create (task, "task", "task", 1, task_space, sizeof (task_space));
}
