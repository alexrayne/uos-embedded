/*
 * Startup initialization and exception handlers for STM32L3xx family.
 *
 * Copyright (C) 2017 Dmitry Podkhvatilin, <vatilin@gmail.com>
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You can redistribute this file and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software Foundation;
 * either version 2 of the License, or (at your discretion) any later version.
 * See the accompanying file "COPYING.txt" for more details.
 *
 * As a special exception to the GPL, permission is granted for additional
 * uses of the text contained in this file.  See the accompanying file
 * "COPY-UOS.txt" for details.
 */
#include <runtime/lib.h>


extern void main (void);

void init_sysclk()
{
    if (KHZ > 24000 && KHZ <= 48000)
        FLASH->ACR |= FLASH_LATENCY(1);
    else if (KHZ > 48000)
        FLASH->ACR |= FLASH_LATENCY(2);

#if defined(CLK_SOURCE_HSI)

#if KHZ < 8000 || KHZ > 64000
#   error "Unsupported HSI frequency!"

#elif KHZ > 8000

#if KHZ % 4000 != 0
#   error "KHZ must be multiple of 4000!"
#endif

    RCC->CFGR = RCC_PLLSRC_HSI_DIV2 | RCC_PLLMUL(KHZ/4000);
    RCC->CR |= RCC_PLLON;
    while (! (RCC->CR & RCC_PLLRDY));
    
    RCC->CFGR |= RCC_SW_PLL;
    while ((RCC->CFGR & RCC_SWS_MASK) != RCC_SWS_PLL);
#endif

#elif defined(CLK_SOURCE_HSE)

#if KHZ > 72000 
#   error "Unsupported HSE frequency!"
#endif

#if KHZ % KHZ_CLKIN != 0
#   error "KHZ must be multiple of KHZ_CLKIN!"
#endif

    RCC->CR |= RCC_HSEON;
    while (! (RCC->CR & RCC_HSERDY));

    RCC->CFGR = RCC_PLLSRC_HSE | RCC_PLLMUL(KHZ/KHZ_CLKIN);
    RCC->CR |= RCC_PLLON;
    while (! (RCC->CR & RCC_PLLRDY));
    
    RCC->CFGR |= RCC_SW_PLL;
    while ((RCC->CFGR & RCC_SWS_MASK) != RCC_SWS_PLL);
#endif // defined(CLK_SOURCE_HSI)
}

/*
 * Initialize the system configuration, cache, intermal SRAM,
 * and set up the stack. Then call main().
 * _init_ is called from gcrt1.S.
 * Attribute "naked" skips function prologue.
 */
void __attribute ((naked))
_init_ (void)
{
	arm_set_basepri (64);

    init_sysclk();

#ifndef NDEBUG
    // Init debug UART
    /*
    RCC->AHBENR |= RCC_IOPCEN;
    GPIOC->MODER |= GPIO_ALT(10) | GPIO_ALT(11);
    GPIOC->AFRH = (GPIOC->AFRH & ~((0xF << (2*4)) | (0xF << (3*4))))
        | (0x7 << (2*4)) | (0x7 << (3*4));
    
    RCC->APB1ENR |= RCC_USART3EN;
    USART3->BRR = KHZ * 1000 / 115200;
    USART3->CR2 = USART_STOP_1;
    USART3->CR1 = USART_UE | USART_TE | USART_RE;
    */
    
    RCC->AHBENR |= RCC_IOPAEN;
    GPIOA->MODER |= GPIO_ALT(9) | GPIO_ALT(10);
    GPIOA->AFRH = (GPIOA->AFRH & ~((0xF << (1*4)) | (0xF << (2*4))))
        | (0x7 << (1*4)) | (0x7 << (2*4));
    
    RCC->APB2ENR |= RCC_USART1EN;
    USART1->BRR = KHZ * 1000 / 115200;
    USART1->CR2 = USART_STOP_1;
    USART1->CR1 = USART_UE | USART_TE | USART_RE;
    
#endif // NDEBUG
        
	arch_init_ram();
	
	/* Initialize priority of exceptions.
	 * Only faults and SVC are permitted when interrupts are disabled
	 * (priority level = 0).  All other interrupts have level 64. */
	ARM_SCB->SHPR1 = ARM_SHPR1_UFAULT(0) |	/* usage fault */
			 ARM_SHPR1_BFAULT(0) |	/* bus fault */
			 ARM_SHPR1_MMFAULT(0);	/* memory management fault */
			 ARM_SCB->SHPR2 = ARM_SHPR2_SVCALL(0);	/* SVCall */
			 ARM_SCB->SHPR3 = ARM_SHPR3_SYSTICK(64) | /* SysTick */
			 ARM_SHPR3_PENDSV(0);	/* PendSV */

    int i;
    for (i = 0; i < (ARCH_INTERRUPTS + 3) / 4; ++i)
        ARM_NVIC_IPR(i) = 0x40404040;

	main ();
}

/*
 * Check memory address.
 * Board-dependent function, should be replaced by user.
 */
bool_t __attribute ((weak))
uos_valid_memory_address (void *ptr)
{
	unsigned u = (unsigned) ptr;

	if (u >= ARM_SRAM_BASE && u < ARM_SRAM_BASE + ARM_SRAM_SIZE)
		return 1;
	return 0;
}

/*
 * This routine should be supplied by user.
 * Implementation of watchdog is different on different boards.
 */
void __attribute ((weak))
watchdog_alive ()
{
	/*TODO*/
}

static void dump_of_death (unsigned *frame, unsigned sp, unsigned ipsr)
{
	debug_printf ("r0 = %08x     r5 = %08x     r10 = %08x     pc   = %08x\n",
		       frame[9],     frame[1],     frame[6],      frame[15]);
	debug_printf ("r1 = %08x     r6 = %08x     r11 = %08x     xpsr = %08x\n",
		       frame[10],    frame[2],     frame[7],      frame[16]);
	debug_printf ("r2 = %08x     r7 = %08x     r12 = %08x     ipsr = %08x\n",
		       frame[11],    frame[3],     frame[13],     ipsr);
	debug_printf ("r3 = %08x     r8 = %08x     sp  = %08x  basepri = %08x\n",
		       frame[12],    frame[4],     sp,  frame[8]);
	debug_printf ("r4 = %08x     r9 = %08x     lr  = %08x\n",
		       frame[0],     frame[5],     frame[14]);

	/* Reset the system. */
	debug_printf ("\nReset...\n\n");
	mdelay (1000);

	for (;;)
		asm volatile ("dmb");
}

void __attribute__ ((naked))
_fault_ ()
{
	/* Save registers in stack. */
	asm volatile (
	"mrs	r12, basepri \n\t"
	"push	{r4-r12}");

	unsigned *frame = arm_get_stack_pointer ();
	unsigned ipsr = arm_get_ipsr ();
	char *message = "fault";
	switch (ipsr) {
	case 2:  message = "non-maskable interrupt"; break;
	case 3:  message = "hard fault"; break;
        case 4:  message = "memory management fault"; break;
        case 5:  message = "bus fault"; break;
        case 6:  message = "usage fault"; break;
        case 14: message = "software interrupt"; break;
	}
	
	debug_printf ("\n\n*** 0x%08x: %s\n\n",
		frame[15], message);
	dump_of_death (frame, (unsigned)arm_get_stack_pointer(), ipsr);
}

void __attribute__ ((naked))
_unexpected_interrupt_ ()
{
	asm volatile ("push	{r4-r11}");

	unsigned *frame = arm_get_stack_pointer ();
	unsigned ipsr = arm_get_ipsr ();

	debug_printf ("\n\n*** 0x%08x: unexpected interrupt #%d\n\n",
		frame[14], ipsr);
	dump_of_death (frame, (unsigned)arm_get_stack_pointer(), ipsr);
}

void __attribute__ ((naked, weak))
_svc_ ()
{
	/* This is needed when no kernel is present. */
}

void __attribute__ ((naked, weak))
_irq_handler_ ()
{
	/* This is needed when no kernel is present. */
}
