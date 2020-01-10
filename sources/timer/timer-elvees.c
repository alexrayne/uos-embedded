#include <timer/timer-private.h>

static inline void
do_set_period(unsigned long itperiod)
{
    /* Use interval timer with prescale 1:1. */
    MC_ITCSR = 0;
    MC_ITSCALE = 0;
    MC_ITPERIOD = itperiod;
    MC_ITCSR = MC_ITCSR_EN;
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
    MC_ITCSR &= ~MC_ITCSR_INT;
}
