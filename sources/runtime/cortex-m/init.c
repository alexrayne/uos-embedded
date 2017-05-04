/*
 * Startup initialization and exception handlers for Cortex-M3 architecture.
 *
 * Copyright (C) 2010 Serge Vakulenko, <serge@vak.ru>
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


extern unsigned long _etext, __data_start, _edata, __bss_start, __bss_end;
extern void main (void);

#if defined (ARM_STM32L151RC) || defined (ARM_STM32L152RC)
static inline void disable_bor()
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

static inline void enable_flash_prefetch()
{
	FLASH->ACR = FLASH_ACC64;
	while (! (FLASH->ACR & FLASH_ACC64));
	FLASH->ACR = FLASH_ACC64 | FLASH_PRFTEN | FLASH_LATENCY(1);
	while (! (FLASH->ACR & FLASH_LATENCY(1)));
}
#endif

#if defined (ARM_STM32L151RC)
void stm32l_low_power()
{
	RCC->AHBENR |= RCC_GPIOAEN | RCC_GPIOBEN | RCC_GPIOCEN;
    RCC->APB1ENR = RCC_PWREN;
    asm volatile ("dsb");
    
    RCC->CSR &= ~RCC_LSION;
    
    GPIOA->MODER = 0xFFFFFFFF;
    GPIOB->MODER = 0xFFFF33FF;
    GPIOC->MODER = 0xFFFFFFFF;
    
    GPIOA->OTYPER = 0;
    GPIOB->OTYPER = 0;
    GPIOC->OTYPER = 0;

    GPIOA->PUPDR = 0;
    GPIOB->PUPDR = 0;
    GPIOC->PUPDR = 0;

    GPIOA->OSPEEDR = 0;
    GPIOB->OSPEEDR = 0;
    GPIOC->OSPEEDR = 0;
    
	//RCC->AHBENR &= ~(RCC_GPIOAEN | RCC_GPIOBEN | RCC_GPIOCEN;
    
    while (PWR->CSR & PWR_VOSF);
    PWR->CR = (PWR->CR & ~PWR_VOS_MASK) | PWR_VOS_1_2 | PWR_ULP | PWR_LPSDSR;
    while (PWR->CSR & PWR_VOSF);
}
#endif

#if defined (ARM_STM32L152RC)
void stm32l_low_power()
{
	RCC->AHBENR |= RCC_GPIOAEN | RCC_GPIOBEN | RCC_GPIOCEN | RCC_GPIODEN |
        RCC_GPIOEEN | RCC_GPIOHEN;
    RCC->APB1ENR = RCC_PWREN;
    asm volatile ("dsb");
    
    RCC->CSR &= ~RCC_LSION;
    
    GPIOA->MODER = 0xFFFFFFFF;
    GPIOB->MODER = 0xFFFFFFFF;
    GPIOC->MODER = 0xFFFFFFFF;
    GPIOD->MODER = 0xFFFFFFFF;
    GPIOE->MODER = 0xFFFFFFFF;
    GPIOH->MODER = 0xFFFFFFFF;
    
    GPIOA->OTYPER = 0;
    GPIOB->OTYPER = 0;
    GPIOC->OTYPER = 0;
    GPIOD->OTYPER = 0;
    GPIOE->OTYPER = 0;
    GPIOH->OTYPER = 0;

    GPIOA->PUPDR = 0;
    GPIOB->PUPDR = 0;
    GPIOC->PUPDR = 0;
    GPIOD->PUPDR = 0;
    GPIOE->PUPDR = 0;
    GPIOH->PUPDR = 0;

    GPIOA->OSPEEDR = 0;
    GPIOB->OSPEEDR = 0;
    GPIOC->OSPEEDR = 0;
    GPIOD->OSPEEDR = 0;
    GPIOE->OSPEEDR = 0;
    GPIOH->OSPEEDR = 0;
    
	//RCC->AHBENR &= ~(RCC_GPIOAEN | RCC_GPIOBEN | RCC_GPIOCEN | RCC_GPIODEN |
    //    RCC_GPIOEEN | RCC_GPIOHEN);
    
    while (PWR->CSR & PWR_VOSF);
    PWR->CR = (PWR->CR & ~PWR_VOS_MASK) | PWR_VOS_1_2 | PWR_ULP | PWR_LPSDSR;
    while (PWR->CSR & PWR_VOSF);
}
#endif

/*
 * Initialize the system configuration, cache, intermal SRAM,
 * and set up the stack. Then call main().
 * _init_ is called from gcrt1.S.
 * Attribute "naked" skips function prologue.
 */
void __attribute ((naked))
_init_ (void)
{
	unsigned long *src, *dest, *limit;
	
// Disable interrupts
#if defined (ARM_CORTEX_M1)
	arm_set_primask(1);
#else
	arm_set_basepri (64);
#endif

#if defined (ARM_1986BE9) || defined (ARM_1986BE1)
	/* Enable JTAG A and B debug ports. */
//	ARM_BACKUP->BKP_REG_0E |= ARM_BKP_REG_0E_JTAG_A | ARM_BKP_REG_0E_JTAG_B;
//	ARM_RSTCLK->PER_CLOCK |= ARM_PER_CLOCK_GPIOB;
#ifdef ARM_1986BE1
        ARM_RSTCLK->PER_CLOCK |= ARM_PER_CLOCK_EEPROM | ARM_PER_CLOCK_BKP;
        /* В зависимости от частоты изменим устанавливаем
        * значение режима работы регулятора 1,8 В
        * и задержку памяти программ при чтении
        */
	if (KHZ > 132000){
		ARM_EEPROM->CMD = (6 << 3);
		ARM_BACKUP->BKP_REG_0E |= 5;
	} else if (KHZ > 125000){
		ARM_EEPROM->CMD = (5 << 3);
		ARM_BACKUP->BKP_REG_0E |= 5;
	} else if (KHZ > 100000) {
		ARM_EEPROM->CMD = (4 << 3);
		ARM_BACKUP->BKP_REG_0E |= 5;
	} else if (KHZ > 80000){
		ARM_EEPROM->CMD = (3 << 3);
		ARM_BACKUP->BKP_REG_0E |= 5;
	} else {
		ARM_EEPROM->CMD = (3 << 3);
		ARM_BACKUP->BKP_REG_0E |= 3;
	}
	ARM_RSTCLK->PER_CLOCK &= ~ARM_PER_CLOCK_EEPROM;
#endif
#ifndef SETUP_HCLK_HSI
	/* Enable HSE generator. */
#ifdef ARM_EXT_GEN
	ARM_RSTCLK->HS_CONTROL = ARM_HS_CONTROL_HSE_ON | ARM_HS_CONTROL_HSE_BYP; 	// HSE External Generator
#else
	ARM_RSTCLK->HS_CONTROL = ARM_HS_CONTROL_HSE_ON;								// HSE External Oscillator
#endif

	while (! (ARM_RSTCLK->CLOCK_STATUS & ARM_CLOCK_STATUS_HSE_RDY))
		continue;

	/* Use HSE for CPU_C1 clock. */
	ARM_RSTCLK->CPU_CLOCK = ARM_CPU_CLOCK_C1_HSE;

	/* Setup PLL for CPU. */
	ARM_RSTCLK->PLL_CONTROL = ARM_PLL_CONTROL_CPU_MUL (KHZ / KHZ_CLKIN);
	ARM_RSTCLK->PLL_CONTROL = ARM_PLL_CONTROL_CPU_MUL (KHZ / KHZ_CLKIN) |
		ARM_PLL_CONTROL_CPU_ON;
	ARM_RSTCLK->PLL_CONTROL = ARM_PLL_CONTROL_CPU_MUL (KHZ / KHZ_CLKIN) |
		ARM_PLL_CONTROL_CPU_ON | ARM_PLL_CONTROL_CPU_RLD;
	ARM_RSTCLK->PLL_CONTROL = ARM_PLL_CONTROL_CPU_MUL (KHZ / KHZ_CLKIN) |
		ARM_PLL_CONTROL_CPU_ON;
	while (! (ARM_RSTCLK->CLOCK_STATUS & ARM_CLOCK_STATUS_PLL_CPU_RDY))
		continue;
#endif

	/* Use PLLCPUo for CPU_C2, CPU_C3 and HCLK. */
	ARM_RSTCLK->CPU_CLOCK = ARM_CPU_CLOCK_C3_C2 |
				ARM_CPU_CLOCK_C2_PLLCPUO |
				ARM_CPU_CLOCK_C1_HSE |
#ifdef SETUP_HCLK_HSI
				ARM_CPU_CLOCK_HCLK_HSI;
#else
				ARM_CPU_CLOCK_HCLK_C3;
#endif

#ifndef NDEBUG
#ifdef ARM_UART1_DEBUG
	/* Set UART1 for debug output. */
	milandr_init_pin (UART1_RX_GPIO, PORT(UART1_RX), PIN(UART1_RX), UART1_RX_FUNC);
	milandr_init_pin (UART1_TX_GPIO, PORT(UART1_TX), PIN(UART1_TX), UART1_TX_FUNC);
	ARM_RSTCLK->PER_CLOCK |= ARM_PER_CLOCK_UART1;	// вкл. тактирования UART1

	/* Set baud rate divisor: 115200 bit/sec. */
#ifdef SETUP_HCLK_HSI
	ARM_RSTCLK->UART_CLOCK = ARM_UART_CLOCK_EN1 |	// разрешаем тактирование UART1
		ARM_UART_CLOCK_BRG1(0);			// HCLK (8 МГц)
	ARM_UART1->IBRD = ARM_UART_IBRD (8000000, 115200);
	ARM_UART1->FBRD = ARM_UART_FBRD (8000000, 115200);
#else
	ARM_RSTCLK->UART_CLOCK = ARM_UART_CLOCK_EN1 |	// разрешаем тактирование UART1
		ARM_UART_CLOCK_BRG1(2);			// HCLK/4 (KHZ/4)
	ARM_UART1->IBRD = ARM_UART_IBRD (KHZ*1000/4, 115200);
	ARM_UART1->FBRD = ARM_UART_FBRD (KHZ*1000/4, 115200);
#endif
	/* Enable UART1, transmiter only. */
	ARM_UART1->LCRH = ARM_UART_LCRH_WLEN8;		// длина слова 8 бит
	ARM_UART1->CTL = ARM_UART_CTL_UARTEN |		// пуск приемопередатчика
			ARM_UART_CTL_TXE;		// передача разрешена

#else
/* Set UART2 for debug output. */
	milandr_init_pin (UART2_RX_GPIO, PORT(UART2_RX), PIN(UART2_RX), UART2_RX_FUNC);
	milandr_init_pin (UART2_TX_GPIO, PORT(UART2_TX), PIN(UART2_TX), UART2_TX_FUNC);
	ARM_RSTCLK->PER_CLOCK |= ARM_PER_CLOCK_UART2;	// вкл. тактирования UART2

/* Set baud rate divisor: 115200 bit/sec. */
#ifdef SETUP_HCLK_HSI
	ARM_RSTCLK->UART_CLOCK = ARM_UART_CLOCK_EN2 |	// разрешаем тактирование UART2
		ARM_UART_CLOCK_BRG2(0);			// HCLK (8 МГц)
	ARM_UART2->IBRD = ARM_UART_IBRD (8000000, 115200);
	ARM_UART2->FBRD = ARM_UART_FBRD (8000000, 115200);
#else
	ARM_RSTCLK->UART_CLOCK = ARM_UART_CLOCK_EN2 |	// разрешаем тактирование UART2
		ARM_UART_CLOCK_BRG2(2);			// HCLK/4 (KHZ/4)
	ARM_UART2->IBRD = ARM_UART_IBRD (KHZ*1000/4, 115200);
	ARM_UART2->FBRD = ARM_UART_FBRD (KHZ*1000/4, 115200);
#endif
/* Enable UART2, transmiter only. */
	ARM_UART2->LCRH = ARM_UART_LCRH_WLEN8;		// длина слова 8 бит
	ARM_UART2->CTL = ARM_UART_CTL_UARTEN |		// пуск приемопередатчика
			ARM_UART_CTL_TXE;		// передача разрешена
#endif /* ARM_UART1_DEBUG */
#endif /* NDEBUG */

#endif /* ARM_1986BE9 || ARM_1986BE1 */


#ifdef ARM_STM32F4

#if KHZ_CLKIN < 2000
#error KHZ_CLKIN < 2 MHz! PLLCFGR will not be properly configured!
#endif
#if ((KHZ / 1000 * 2 / 48) * 48) != (KHZ / 1000 * 2)
#warning PLL48CLK is not equal 48 MHz, USB OTG FS will not work properly!
#endif
#if (KHZ / 1000 * 2) / (KHZ / 1000 * 2 / 48) > 48
#warning PLL48CLK is not less or equal 48 MHz, SDIO and random number \
generator will not work properly
#endif

    // Initialize PLL
    RCC->PLLCFGR = RCC_PLLM(KHZ_CLKIN / 1000) | RCC_PLLN(KHZ / 1000 * 2) |
        RCC_PLLP_2 | RCC_PLLSRC_HSE | RCC_PLLQ(KHZ / 1000 * 2 / 48);
        
    RCC->CR |= RCC_PLLON | RCC_HSEON;
    while (! (RCC->CR & (RCC_PLLRDY | RCC_HSERDY)));
    
    // We have to increase embedded flash wait states when 
    // frequency is high
    FLASH_IFACE->ACR = FLASH_LATENCY(5) | FLASH_PRFTEN |
        FLASH_ICEN | FLASH_DCEN;
    
    // Switch core to PLL clocks. Frequency of AHB is set to HCLK,
    // APB1 - to HCLK/2, APB2 - to HCLK/4
    RCC->CFGR = RCC_SW_PLL | RCC_HPRE_NODIV | RCC_PPRE1_DIV4 |
        RCC_PPRE2_DIV2;
    while (RCC->CFGR & RCC_SWS_MASK != RCC_SWS_PLL);

#ifndef NDEBUG
    // Init debug UART
    RCC->AHB1ENR |= RCC_GPIOCEN;
    GPIOC->MODER |= GPIO_ALT(10) | GPIO_ALT(11);
    GPIOC->AFRH |= GPIO_AF_USART3(10) | GPIO_AF_USART3(11);
    
    unsigned mant = (unsigned)(KHZ / 4 / (115.2 * 16));
    unsigned frac = (KHZ / 4 / (115.2 * 16) - mant) * 16;
    RCC->APB1ENR |= RCC_USART3EN;
    USART3->CR1 |= USART_UE;
    USART3->CR2 |= USART_STOP_1;
    USART3->BRR = USART_DIV_MANTISSA(mant) | USART_DIV_FRACTION(frac);
    USART3->CR1 |= USART_TE | USART_RE;
#endif // NDEBUG
#endif // ARM_STM32F4


#if defined(ARM_STM32L151RC) || defined(ARM_STM32L152RC)

#if defined(CLK_SOURCE_HSI)

	enable_flash_prefetch();

    RCC->CR |= RCC_HSION;
    while (! (RCC->CR & RCC_HSIRDY));

#if KHZ==16384
    RCC->CFGR = RCC_SW_HSI;
    while ((RCC->CFGR & RCC_SWS_MASK) != RCC_SWS_HSI);
#elif KHZ==24576
	RCC->CFGR = RCC_PLLSRC_HSI | RCC_PLLMUL6 | RCC_PLLDIV4;
    RCC->CR |= RCC_PLLON;
    while (! (RCC->CR & RCC_PLLRDY));
    
    RCC->CFGR |= RCC_SW_PLL;
    while ((RCC->CFGR & RCC_SWS_MASK) != RCC_SWS_PLL);
#elif KHZ==32768
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

	enable_flash_prefetch();

    RCC->CR |= RCC_HSEON;
    while (! (RCC->CR & RCC_HSERDY));
	
#if KHZ_HSE==2048
	RCC->CFGR = RCC_PLLSRC_HSE | RCC_PLLMUL48 | RCC_PLLDIV3;
#elif KHZ_HSE==4096
	RCC->CFGR = RCC_PLLSRC_HSE | RCC_PLLMUL24 | RCC_PLLDIV3;
#elif KHZ_HSE==8192
	RCC->CFGR = RCC_PLLSRC_HSE | RCC_PLLMUL12 | RCC_PLLDIV3;
#elif KHZ_HSE==12288
	RCC->CFGR = RCC_PLLSRC_HSE | RCC_PLLMUL8 | RCC_PLLDIV3;
#elif KHZ_HSE==16384
	RCC->CFGR = RCC_PLLSRC_HSE | RCC_PLLMUL6 | RCC_PLLDIV3;
#elif KHZ_HSE==24576
	RCC->CFGR = RCC_PLLSRC_HSE | RCC_PLLMUL4 | RCC_PLLDIV3;
#else
#	error "Unsupported HSE frequency"
#endif // KHZ_HSE==

#if KHZ!=32768
#	error "Unsupported CPU frequency when using HSE"
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
#	ifndef POWER_SAVE
		enable_flash_prefetch();
#	endif
#else
#	error "Invalid clock frequency for MSI"
#endif	// KHZ==

#endif	// CLK_SOURCE_HSI
    
#ifdef POWER_SAVE
	stm32l_low_power();
	disable_bor();
#endif

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
#endif // ARM_STM32L151RC || ARM_STM32L152RC

#ifndef EMULATOR /* not needed on emulator */
	/* Copy the .data image from flash to ram.
	 * Linker places it at the end of .text segment. */
	src = &_etext;
	dest = &__data_start;
	limit = &_edata;
	while (dest < limit)
		*dest++ = *src++;
#endif
	/* Initialize .bss segment by zeroes. */
	dest = &__bss_start;
	limit = &__bss_end;
	while (dest < limit)
		*dest++ = 0;
		
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

#if 0       
	ARM_NVIC_IPR(0) = 0x40404040;		/* CAN1, CAN2, USB */
	ARM_NVIC_IPR(1) = 0x40404040;		/* DMA, UART1, UART2 */
	ARM_NVIC_IPR(2) = 0x40404040;		/* SSP1, I2C, POWER */
	ARM_NVIC_IPR(3) = 0x40404040;		/* WWDG, Timer1, Timer2 */
	ARM_NVIC_IPR(4) = 0x40404040;		/* Timer3, ADC, COMPARATOR */
	ARM_NVIC_IPR(5) = 0x40404040;		/* SSP2 */
	ARM_NVIC_IPR(6) = 0x40404040;		/* BACKUP */
	ARM_NVIC_IPR(7) = 0x40404040;		/* external INT[1:4] */
#endif

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
#if defined(ARM_1986BE1)
	if (u >= 0x20100000 && u < 0x20104000)
		return 1;
#endif
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
#ifdef ARM_CORTEX_M1
	asm volatile (
	"push	{r4-r7} \n\t"
	"mov    r1, r8 \n\t"
	"mov    r2, r9 \n\t"
	"mov    r3, r10 \n\t"
	"mov    r4, r11 \n\t"
	"mrs    r5, primask\n\t"
	"push	{r1-r5} \n\t");
#else
	asm volatile (
	"mrs	r12, basepri \n\t"
	"push	{r4-r12}");
#endif

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
#ifdef ARM_CORTEX_M1
	asm volatile (
	"push	{r4-r7} \n\t"
	"mov    r0, r8 \n\t"
	"mov    r1, r9 \n\t"
	"mov    r2, r10 \n\t"
	"mov    r3, r11 \n\t"
	"push	{r0-r3} \n\t");
#else
	asm volatile ("push	{r4-r11}");
#endif

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
