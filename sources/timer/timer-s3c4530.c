#include <timer/timer-private.h>

static inline void
do_set_period(unsigned long tdata)
{
    /* Use timer 0 in toggle mode. */
    ARM_TMOD &= ~(ARM_TMOD_TE0 | ARM_TMOD_TCLR0);
    ARM_TDATA(0) = tdata;
    ARM_TCNT(0) = 0;
    ARM_TMOD |= ARM_TMOD_TE0 | ARM_TMOD_TMD0;
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
