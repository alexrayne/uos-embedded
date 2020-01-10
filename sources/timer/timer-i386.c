#include <timer/timer-private.h>

static inline void
do_set_period(unsigned short count)
{
    outb (I8253_MODE_SEL0 | I8253_MODE_RATEGEN | I8253_MODE_16BIT,
        I8253_MODE);
    outb (count & 0xff, I8253_CNTR0);
    outb (count >> 8, I8253_CNTR0);
}

void
hw_set_period(unsigned long khz, unsigned long new_period)
{
#if defined(NSEC_TIMER)
#error "Nanosecond timer is not supported for I386 architecture!"
#elif defined(USEC_TIMER)
#error "Microsecond timer is not supported for I386 architecture!"
#else
    do_set_period((khz * msec_per_tick + 500) / 1000);
#endif
}

void
hw_clear_interrupt()
{
}
