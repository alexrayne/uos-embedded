/*
 * Precise microsecond delay using SysTick timer.
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

#ifdef ARM_1986BE1
#if (ARM_SYS_TIMER==1)
#	define TIMER_IRQ	TIMER1_IRQn
#	define SYS_TIMER	ARM_TIMER1
#	define PER_CLOCK_EN	ARM_PER_CLOCK_TIMER1
#	define TIM_CLK_EN	ARM_TIM_CLOCK_EN1
#elif (ARM_SYS_TIMER==2)
#	define TIMER_IRQ	TIMER2_IRQn
#	define SYS_TIMER	ARM_TIMER2
#	define PER_CLOCK_EN	ARM_PER_CLOCK_TIMER2
#	define TIM_CLK_EN	ARM_TIM_CLOCK_EN2
#elif (ARM_SYS_TIMER==3)
#	define TIMER_IRQ	TIMER3_IRQn
#	define SYS_TIMER	ARM_TIMER3
#	define PER_CLOCK_EN	ARM_PER_CLOCK_TIMER3
#	define TIM_CLK_EN	ARM_TIM_CLOCK_EN3
#elif (ARM_SYS_TIMER==4)
#	define TIMER_IRQ	TIMER4_IRQn
#	define SYS_TIMER	ARM_TIMER4
#	define PER_CLOCK_EN	ARM_PER_CLOCK_TIMER4
#	define TIM_CLK_EN	ARM_UART_CLOCK_TIM4_EN
#else
#	warning "ARM_SYS_TIMER is not defined in CFLAGS (target.cfg). Using TIMER1 for system timer."
#	define TIMER_IRQ	TIMER1_IRQn
#	define SYS_TIMER	ARM_TIMER1
#	define PER_CLOCK_EN	ARM_PER_CLOCK_TIMER1
#	define TIM_CLK_EN	ARM_TIM_CLOCK_EN1
#endif
#endif


volatile uint32_t __timer_ticks_uos;

#ifdef CALIBRATED_DELAY

#if KHZ <= 2100
#   define  DELAY_QUANT 4
static inline void calib_delay_quant()
{
    asm volatile ("nop; nop; nop; nop;");
}
#elif KHZ <= 4200
#   define  DELAY_QUANT 2
static inline void calib_delay_quant()
{
    asm volatile ("nop; nop;");
}
#elif KHZ <= 16000
#   define  DELAY_QUANT 1
static inline void calib_delay_quant()
{
    asm volatile ("nop; nop; nop; nop; nop;");
}
#elif KHZ <= 32000
#   define  DELAY_QUANT 1
static inline void calib_delay_quant()
{
    asm volatile ("nop; nop; nop; nop; nop;");
    asm volatile ("nop; nop; nop; nop; nop; nop; nop; nop;");
}
#endif

#endif

void udelay (unsigned usec)
{

#ifdef CALIBRATED_DELAY
    if (usec <= DELAY_QUANT)
        return;
        
    usec -= DELAY_QUANT - 1;
    
    while (usec > DELAY_QUANT) {
        calib_delay_quant();
        usec -= DELAY_QUANT;
    }
    
    if (usec)
        calib_delay_quant();
#else // CALIBRATED_DELAY

#ifdef ARM_1986BE1
    if (! usec)
        return;

    if (! (ARM_RSTCLK->PER_CLOCK & PER_CLOCK_EN)) {
        ARM_RSTCLK->PER_CLOCK |= PER_CLOCK_EN;
#if (ARM_SYS_TIMER==4)
        ARM_RSTCLK->UART_CLOCK |= TIM_CLK_EN;
#else
        ARM_RSTCLK->TIM_CLOCK |= TIM_CLK_EN;
#endif
        SYS_TIMER->TIM_ARR = 0xFFFFFFFF;
        SYS_TIMER->TIM_CNT = 0;
        SYS_TIMER->TIM_PSG = 0;
        SYS_TIMER->TIM_IE = 0;
        SYS_TIMER->TIM_CNTRL = ARM_TIM_CNT_EN;
	}
    uint32_t arr = SYS_TIMER->TIM_ARR;

    uint32_t prev_ticks;
    uint32_t ticks = __timer_ticks_uos;
    uint32_t now;

    if (!ticks) {
        if (SYS_TIMER->TIM_STATUS & ARM_TIM_CNT_ZERO_EVENT) {
            SYS_TIMER->TIM_STATUS &= ~ARM_TIM_CNT_ZERO_EVENT;
        }
        now = SYS_TIMER->TIM_CNT;
        if (SYS_TIMER->TIM_STATUS & ARM_TIM_CNT_ZERO_EVENT) {
            SYS_TIMER->TIM_STATUS &= ~ARM_TIM_CNT_ZERO_EVENT;
            now = SYS_TIMER->TIM_CNT;
        }
    } else {
        prev_ticks = ticks;
        now = SYS_TIMER->TIM_CNT;
        ticks = __timer_ticks_uos;
        if (prev_ticks != ticks) {
            now = SYS_TIMER->TIM_CNT;
            prev_ticks = ticks;
        }
    }

    unsigned final = now + (usec-1)*(KHZ / 1000);  // usec-1 нивелирует накладные раходы (вызов функции и т.п.)

    for (;;) {
        if (!ticks) {
            if (SYS_TIMER->TIM_STATUS & ARM_TIM_CNT_ZERO_EVENT) {
                SYS_TIMER->TIM_STATUS &= ~ARM_TIM_CNT_ZERO_EVENT;
                final -= (arr+1);
            }
        } else {
            if (prev_ticks != ticks) {
                uint32_t delta;
                if (ticks < prev_ticks) {
                    delta = ticks - prev_ticks + 1;
                } else {
                    delta = ticks - prev_ticks;
                }
                final -= (arr + 1) * delta;
            }
            prev_ticks = ticks;
            ticks = __timer_ticks_uos;
        }

        /* This comparison is valid only when using a signed type. */
        now = SYS_TIMER->TIM_CNT;
        if ((int) (final - now) < 0) // Считаем вверх
            break;
    }
#else
    if (! usec)
        return;

    uint32_t ctrl;
    uint32_t prev_ticks;
    uint32_t ticks = __timer_ticks_uos;

    prev_ticks = ticks;

    if (!ticks) {
        ctrl = ARM_SYSTICK->CTRL;
        if (! (ctrl & ARM_SYSTICK_CTRL_ENABLE)) {
            /* Start timer using HCLK clock, no interrupts. */
            ARM_SYSTICK->LOAD = 0xFFFFFF;
            ARM_SYSTICK->CTRL = ARM_SYSTICK_CTRL_HCLK |
                    ARM_SYSTICK_CTRL_ENABLE;
        }
    }

    uint32_t load = ARM_SYSTICK->LOAD & 0xFFFFFF;

    uint32_t now;

    if (!ticks) {
        ctrl = ARM_SYSTICK->CTRL;
        now = ARM_SYSTICK->VAL & 0xFFFFFF;
        ctrl = ARM_SYSTICK->CTRL;
        if (ctrl & ARM_SYSTICK_CTRL_COUNTFLAG) {
            now = ARM_SYSTICK->VAL & 0xFFFFFF;
        }
    } else {
        prev_ticks = ticks;
        now = ARM_SYSTICK->VAL & 0xFFFFFF;
        ticks = __timer_ticks_uos;
        if (prev_ticks != ticks) {
            now = ARM_SYSTICK->VAL & 0xFFFFFF;
            prev_ticks = ticks;
        }
    }

#ifdef SETUP_HCLK_HSI
    uint32_t final = now - (usec-1) * 8;
#else
    uint32_t final = now - (usec-1) * (KHZ / 1000); // usec-1 нивелирует накладные раходы (вызов функции и т.п.)
#endif
    for (;;) {
        if (!ticks) {
            ctrl = ARM_SYSTICK->CTRL;
            if (ctrl & ARM_SYSTICK_CTRL_COUNTFLAG) {
                final += (load+1);
            }
        } else {
            if (prev_ticks != ticks) {
                uint32_t delta;
                if (ticks < prev_ticks) {
                    delta = ticks - prev_ticks + 1;
                } else {
                    delta = ticks - prev_ticks;
                }
                final += (load + 1) * delta;
            }
            prev_ticks = ticks;
            ticks = __timer_ticks_uos;
        }
        /* This comparison is valid only when using a signed type. */
        now = ARM_SYSTICK->VAL & 0xFFFFFF;
        if ((int) (now - final) < 0) // Считаем вниз
            break;
    }
#endif

#endif // CALIBRATED_DELAY
}
