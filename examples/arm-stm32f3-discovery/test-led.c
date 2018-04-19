/*
 * Проверка светодиодов на отладочной плате
 */
#include <runtime/lib.h>

#define LED_ON(n) (GPIOE->BSRR = GPIO_SET(8 + n))
#define LED_OFF(n) (GPIOE->BSRR = GPIO_RESET(8 + n))

void init_leds ()
{
    RCC->AHBENR |= RCC_IOPEEN;
    GPIOE->MODER = GPIO_OUT(8) | GPIO_OUT(9) | GPIO_OUT(10) | GPIO_OUT(11) |
        GPIO_OUT(12) | GPIO_OUT(13) | GPIO_OUT(14) | GPIO_OUT(15);
}

int main (void)
{
	init_leds ();

	int i = 0;	
	for (;;) {
		LED_OFF(i);
		i = (i + 1) % 8;
		LED_ON(i);
		mdelay (500);
	}
}
