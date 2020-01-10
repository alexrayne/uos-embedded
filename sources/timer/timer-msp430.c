#include <timer/timer-private.h>

static inline void
do_set_period(unsigned long divider)
{
#ifdef TAEX0
    /* Setup divider expansion. */
    small_uint_t nx = (divider - 4) >> (16 + 3);
    assert (nx <= 7);
    TAEX0 = nx;
    if (nx)
        divider /= nx + 1;
#endif
    TACTL = TACLR;              /* Stop timer. */
    TACTL = TASSEL_SMCLK | ID_DIV8;     /* Source clock SMCLK divided by 8. */
    divider = (divider + 4) >> 3;
    assert (divider <= 65536);
    TACCR0 = divider - 1;           /* Set tick rate. */
    TACTL |= MC_1;              /* Start timer in up mode. */
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
}
