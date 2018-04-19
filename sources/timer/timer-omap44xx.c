#include <timer/timer-private.h>

static inline void
do_set_period(arm_reg_t load)
{
    ARM_PRT_LOAD = load;
    ARM_PRT_CONTROL = ARM_PRT_TIMER_EN | ARM_PRT_AUTO_RELOAD |
        ARM_PRT_IRQ_EN;
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
    ARM_PRT_INT_STATUS = ARM_PRT_EVENT;
}
