#include <timer/timer-private.h>

static inline void
do_set_period(arm_reg_t load)
{
    ARM_SYSTICK->CTRL = 0;
    ARM_SYSTICK->VAL = 0;
    ARM_SYSTICK->LOAD = load;
    ARM_SYSTICK->CTRL = ARM_SYSTICK_CTRL_ENABLE |
                ARM_SYSTICK_CTRL_TICKINT |
                ARM_SYSTICK_CTRL_HCLK;
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
}
