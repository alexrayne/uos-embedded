/*
 * Проверка светодиодов на отладочной плате
 */
#include <runtime/lib.h>

void init_leds ()
{
    RCC->AHB1ENR |= RCC_GPIOAEN;
    GPIOA->MODER = GPIO_OUT(5);
}

int main (void)
{
	init_leds ();

	int i = 0;	
	for (;;) {
	    if (i & 1)
	        GPIOA->BSRR = GPIO_SET(5);
	    else
	        GPIOA->BSRR = GPIO_RESET(5);
		mdelay (500);
		++i;
	}
}
