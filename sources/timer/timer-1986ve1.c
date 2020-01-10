#include <timer/timer-private.h>

static inline void
do_set_period(arm_reg_t arr)
{
    ARM_RSTCLK->PER_CLOCK |= PER_CLOCK_EN;
#if (ARM_SYS_TIMER==4)
    ARM_RSTCLK->UART_CLOCK |= TIM_CLK_EN;
#else
    ARM_RSTCLK->TIM_CLOCK |= TIM_CLK_EN;
#endif
    SYS_TIMER->TIM_CNT = 0;
    SYS_TIMER->TIM_PSG = 0;
    SYS_TIMER->TIM_ARR = arr;
    SYS_TIMER->TIM_IE = ARM_TIM_CNT_ARR_EVENT_IE;
    SYS_TIMER->TIM_CNTRL = ARM_TIM_CNT_EN;
}

void
hw_set_period(unsigned long khz, unsigned long new_period)
{
#if defined(NSEC_TIMER)
    do_set_period((uint64_t) khz * new_period / 1000000 - 1);
#elif defined(USEC_TIMER)
    do_set_period(khz * new_period / 1000 - 1);
#else
    do_set_period(khz * new_period - 1);
#endif
}

void
hw_clear_interrupt()
{
    SYS_TIMER->TIM_STATUS &= ~ARM_TIM_CNT_ARR_EVENT;
}
