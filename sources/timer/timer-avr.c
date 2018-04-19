#include <timer/timer-private.h>

#if __AVR__

static inline void
do_set_period(unsigned long ocr1a)
{
    TCCR1A = 0;
    TCCR1B = 0;
    OCR1A = ocr1a;
    TCNT1 = 0;
    TCCR1B = 0x0A;  /* clock source CK/8, clear on match A */
}

void
hw_set_period(unsigned long khz, unsigned long new_period)
{
#if defined(NSEC_TIMER)
#error "Nanosecond timer is not supported for AVR architecture!"
#elif defined(USEC_TIMER)
#error "Microsecond timer is not supported for AVR architecture!"
#else
    do_set_period((khz * new_period) / 8 - 2);
#endif
}

void
hw_clear_interrupt()
{
}

#endif
