/*
 * Проверка светодиодов на отладочной плате
 */
 
#include "runtime/lib.h"
#include "kernel/uos.h"
#include "timer/timer.h"

ARRAY (task_space, 0x400);	/* Memory for task stack */
timer_t timer;

void init_leds ()
{
    RCC->AHB1ENR |= RCC_GPIOAEN;
    GPIOA->MODER = GPIO_OUT(5);
}

void task (void *arg)
{
	int i = 0;	
	for (;;) {
	    if (i & 1)
	        GPIOA->BSRR = GPIO_SET(5);
	    else
	        GPIOA->BSRR = GPIO_RESET(5);
		++i;
		
		timer_delay(&timer, 2000);
		//mdelay(500);
	}
}

void uos_init (void)
{
    init_leds ();
    timer_init(&timer, KHZ, 100);
	task_create (task, "task", "task", 1, task_space, sizeof (task_space));
}

