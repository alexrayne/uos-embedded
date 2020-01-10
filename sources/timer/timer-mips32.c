#include <timer/timer-private.h>

static inline void
do_set_period(unsigned long load)
{
    unsigned count = mips_read_c0_register (C0_COUNT);
    t->compare_step = (t->khz * t->msec_per_tick + 1) / 2;
    mips_write_c0_register (C0_COMPARE, count + t->compare_step);
}

void
hw_set_period(unsigned long khz, unsigned long new_period)
{
#if defined(NSEC_TIMER)
    do_set_period(((uint64_t) khz * new_period / 1000000 + 1) / 2);
#elif defined(USEC_TIMER)
    do_set_period((khz * new_period / 1000 + 1) / 2);
#else
    do_set_period((khz * new_period + 1) / 2);
#endif
}

void
hw_clear_interrupt()
{
    unsigned compare = mips_read_c0_register (C0_COMPARE);
    do {
        compare += t->compare_step;
        mips_write_c0_register (C0_COMPARE, compare);
    } while ((int) (compare - mips_read_c0_register (C0_COUNT)) < 0);
}
