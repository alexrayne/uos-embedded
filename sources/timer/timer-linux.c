#include <timer/timer-private.h>

static inline void
do_set_period(unsigned long usec)
{
    struct itimerval itv;

    itv.it_interval.tv_sec = 0;
    itv.it_interval.tv_usec = usec;
    itv.it_value = itv.it_interval;
    setitimer (ITIMER_REAL, &itv, 0);
}

void
hw_set_period(unsigned long khz, unsigned long new_period)
{
#if defined(NSEC_TIMER)
    do_set_period(new_period / 1000);
#elif defined(USEC_TIMER)
    do_set_period(new_period);
#else
    do_set_period(new_period * 1000);
#endif
}

void
hw_clear_interrupt()
{
}
