/*
 * Startup initialization and exception handlers for STM32L1xx family.
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

static inline void stm32l_disable_bor()
{
    if ((FLASH->OBR & FLASH_BOR_OFF_MASK) == 0) {
        return;
    }
        
    /* Unlocks the option bytes block access */
    FLASH->PEKEYR = FLASH_PEKEY1;
    FLASH->PEKEYR = FLASH_PEKEY2;
    FLASH->OPTKEYR = FLASH_OPTKEY1;
    FLASH->OPTKEYR = FLASH_OPTKEY2;

    /* Clears the FLASH pending flags */
    FLASH->SR = FLASH_EOP | FLASH_WRPERR | FLASH_PGAERR | FLASH_SIZERR | FLASH_OPTVERR;
    
    /* Set BOR OFF */
    uint32_t tmp = 0, tmp1 = 0;
    tmp1 = (FLASH->OBR & 0x00F00000) >> 16;
    tmp = (uint32_t)~(0 | tmp1) << 16;
    tmp |= (0 | tmp1);
    
    uint32_t timeout = 0x8000;
    while (FLASH->SR & FLASH_BSY)
        if (--timeout == 0)
            break;
            
    if (FLASH->SR & FLASH_ERR_MASK)
        return;
        
    OB->USER = tmp; 
    
    timeout = 0x8000;
    while (FLASH->SR & FLASH_BSY)
        if (--timeout == 0)
            break;
            
    if (FLASH->SR & FLASH_ERR_MASK)
        return;

    /* Launch the option byte loading */
    FLASH->PECR |= FLASH_OBL_LAUNCH;
}

static inline void stm32l_enable_flash_prefetch()
{
    FLASH->ACR = FLASH_ACC64;
    while (! (FLASH->ACR & FLASH_ACC64));
    FLASH->ACR = FLASH_ACC64 | FLASH_PRFTEN | FLASH_LATENCY(1);
    while (! (FLASH->ACR & FLASH_LATENCY(1)));
}

void stm32l_low_power()
{
    RCC->APB1ENR = RCC_PWREN;
    asm volatile ("dsb");
    
    RCC->CSR &= ~RCC_LSION;
    
    while (PWR->CSR & PWR_VOSF);
    PWR->CR = (PWR->CR & ~PWR_VOS_MASK) | PWR_VOS_1_2 | PWR_ULP | PWR_LPSDSR;
    while (PWR->CSR & PWR_VOSF);
}

void stm32l_init_sysclk()
{
#if defined(CLK_SOURCE_HSI)

    stm32l_enable_flash_prefetch();

    RCC->CR |= RCC_HSION;
    while (! (RCC->CR & RCC_HSIRDY));

#if KHZ==16000
    RCC->CFGR = RCC_SW_HSI;
    while ((RCC->CFGR & RCC_SWS_MASK) != RCC_SWS_HSI);
#elif KHZ==24000
    RCC->CFGR = RCC_PLLSRC_HSI | RCC_PLLMUL6 | RCC_PLLDIV4;
    RCC->CR |= RCC_PLLON;
    while (! (RCC->CR & RCC_PLLRDY));
    
    RCC->CFGR |= RCC_SW_PLL;
    while ((RCC->CFGR & RCC_SWS_MASK) != RCC_SWS_PLL);
#elif KHZ==32000
    RCC->CFGR = RCC_PLLSRC_HSI | RCC_PLLMUL6 | RCC_PLLDIV3;
    RCC->CR |= RCC_PLLON;
    while (! (RCC->CR & RCC_PLLRDY));
    
    RCC->CFGR |= RCC_SW_PLL;
    while ((RCC->CFGR & RCC_SWS_MASK) != RCC_SWS_PLL);
#else
#	error "Unsupported CPU frequency when using HSI"
#endif

    RCC->CR &= ~RCC_MSION;

#elif defined(CLK_SOURCE_HSE)

    stm32l_enable_flash_prefetch();

    RCC->CR |= RCC_HSEON;
    while (! (RCC->CR & RCC_HSERDY));
	
#if KHZ_HSE==2000
    RCC->CFGR = RCC_PLLSRC_HSE | RCC_PLLMUL48 | RCC_PLLDIV3;
#elif KHZ_HSE==4000
    RCC->CFGR = RCC_PLLSRC_HSE | RCC_PLLMUL24 | RCC_PLLDIV3;
#elif KHZ_HSE==8000
    RCC->CFGR = RCC_PLLSRC_HSE | RCC_PLLMUL12 | RCC_PLLDIV3;
#elif KHZ_HSE==12000
    RCC->CFGR = RCC_PLLSRC_HSE | RCC_PLLMUL8 | RCC_PLLDIV3;
#elif KHZ_HSE==16000
    RCC->CFGR = RCC_PLLSRC_HSE | RCC_PLLMUL6 | RCC_PLLDIV3;
#elif KHZ_HSE==24000
    RCC->CFGR = RCC_PLLSRC_HSE | RCC_PLLMUL4 | RCC_PLLDIV3;
#else
#   error "Unsupported HSE frequency"
#endif // KHZ_HSE==

#if KHZ!=32000
#   error "Unsupported CPU frequency when using HSE"
#endif

    RCC->CR |= RCC_PLLON;
    while (! (RCC->CR & RCC_PLLRDY));
    
    RCC->CFGR |= RCC_SW_PLL;
    while ((RCC->CFGR & RCC_SWS_MASK) != RCC_SWS_PLL);

    RCC->CR &= ~RCC_MSION;

#else 	// MSI clock source

#if KHZ==65
    RCC->ICSCR = (RCC->ICSCR & ~RCC_MSIRANGE_MASK) | RCC_MSIRANGE_65536;
    while (! (RCC->CR & RCC_MSIRDY));
#elif KHZ==131
    RCC->ICSCR = (RCC->ICSCR & ~RCC_MSIRANGE_MASK) | RCC_MSIRANGE_131072;
    while (! (RCC->CR & RCC_MSIRDY));
#elif KHZ==262
    RCC->ICSCR = (RCC->ICSCR & ~RCC_MSIRANGE_MASK) | RCC_MSIRANGE_262144;
    while (! (RCC->CR & RCC_MSIRDY));
#elif KHZ==524
    RCC->ICSCR = (RCC->ICSCR & ~RCC_MSIRANGE_MASK) | RCC_MSIRANGE_524288;
    while (! (RCC->CR & RCC_MSIRDY));
#elif KHZ==1048
    RCC->ICSCR = (RCC->ICSCR & ~RCC_MSIRANGE_MASK) | RCC_MSIRANGE_1048K;
    while (! (RCC->CR & RCC_MSIRDY));
#elif KHZ==2097
    // This is reset value!
    //RCC->ICSCR = (RCC->ICSCR & ~RCC_MSIRANGE_MASK) | RCC_MSIRANGE_2097K;
    //while (! (RCC->CR & RCC_MSIRDY));
#elif KHZ==4194
    RCC->ICSCR = (RCC->ICSCR & ~RCC_MSIRANGE_MASK) | RCC_MSIRANGE_4194K;
    while (! (RCC->CR & RCC_MSIRDY));
#       ifndef POWER_SAVE
	    stm32l_enable_flash_prefetch();
#       endif
#else
#	error "Invalid clock frequency for MSI"
#endif	// KHZ==

#endif	// CLK_SOURCE_HSI

    RCC->CFGR |= RCC_HPRE_DIV(KHZ_HCLK / KHZ) | RCC_PPRE1_DIV(KHZ_PCLK1 / KHZ) |
                    RCC_PPRE2_DIV(KHZ_PCLK2 / KHZ);
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
// Disable interrupts
	arm_set_basepri (64);

#ifdef POWER_SAVE
    stm32l_low_power();
    stm32l_disable_bor();
#endif

    stm32l_init_sysclk();

    // Init debug UART    
#ifndef NDEBUG

#ifndef DBG_UART_KHZ
#	define DBG_UART_KHZ 115.2
#endif

    unsigned mant = (unsigned)(KHZ / (DBG_UART_KHZ * 16));
    unsigned frac = (KHZ / (DBG_UART_KHZ * 16) - mant) * 16;
    
#if  defined USE_USART1
#warning Using USART1
    RCC->AHBENR |= RCC_GPIOAEN;
    GPIOA->MODER = (GPIOA->MODER & ~(GPIO_MODE_MASK(9) | GPIO_MODE_MASK(10))) |
		GPIO_ALT(9) | GPIO_ALT(10);
    GPIOA->AFRH |= GPIO_AF_USART1(9) | GPIO_AF_USART1(10);
    RCC->APB2ENR |= RCC_USART1EN;
    USART1->CR1 |= USART_UE;
    USART1->CR2 |= USART_STOP_1;
    USART1->BRR = USART_DIV_MANTISSA(mant) | USART_DIV_FRACTION(frac);
    USART1->CR1 |= USART_TE | USART_RE;
#else
#warning Using USART3
    RCC->AHBENR |= RCC_GPIOCEN;
    GPIOC->MODER = (GPIOC->MODER & ~(GPIO_MODE_MASK(10) | GPIO_MODE_MASK(11))) |
		GPIO_ALT(10) | GPIO_ALT(11);
    GPIOC->AFRH |= GPIO_AF_USART3(10) | GPIO_AF_USART3(11);
    RCC->APB1ENR |= RCC_USART3EN;
    USART3->CR1 |= USART_UE;
    USART3->CR2 |= USART_STOP_1;
    USART3->BRR = USART_DIV_MANTISSA(mant) | USART_DIV_FRACTION(frac);
    USART3->CR1 |= USART_TE | USART_RE;
#endif

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
#ifdef ARM_CORTEX_M1
	debug_printf ("r0 = %08x     r5 = %08x     r10 = %08x     pc   = %08x\n",
		       frame[9],     frame[6],     frame[2],      frame[15]);
	debug_printf ("r1 = %08x     r6 = %08x     r11 = %08x     xpsr = %08x\n",
		       frame[10],    frame[7],     frame[3],      frame[16]);
	debug_printf ("r2 = %08x     r7 = %08x     r12 = %08x     ipsr = %08x\n",
		       frame[11],    frame[8],     frame[13],     ipsr);
	debug_printf ("r3 = %08x     r8 = %08x     sp  = %08x  iser0 = %08x\n",
		       frame[12],    frame[0],     sp,  frame[4]);
	debug_printf ("r4 = %08x     r9 = %08x     lr  = %08x\n",
		       frame[5],     frame[1],     frame[14]);
#else
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
#endif

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
	/* Save registers in stack. */
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
