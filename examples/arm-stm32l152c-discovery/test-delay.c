/*
 * Проверка работы функции задержки
 */
#include <runtime/lib.h>

int main (void)
{
    RCC->AHBENR |= RCC_GPIOAEN;
    asm volatile ("dmb");
    GPIOA->MODER = GPIO_OUT(5);
    
	for (;;) {
	    GPIOA->BSRR = GPIO_SET(5);
	    udelay(1000);
	    GPIOA->BSRR = GPIO_RESET(5);
	}
}
