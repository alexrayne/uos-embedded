#include <timer/timer-private.h>

static inline void
do_set_period(unsigned long pimr_val)
{
    *AT91C_PITC_PIMR = (((pimr_val + 8) >> 4) - 1) |
        AT91C_PITC_PITEN | AT91C_PITC_PITIEN;
}

void
hw_set_period(unsigned long khz, unsigned long new_period)
{
#if defined(NSEC_TIMER)
    do_set_period((uint64_t) khz * new_period / 1000000);
#elif defined(USEC_TIMER)
    do_set_period(khz * new_period / 1000);
#else
    do_set_period(khz * new_period);
#endif
}

void
hw_clear_interrupt()
{
    *AT91C_PITC_PIVR;
}
