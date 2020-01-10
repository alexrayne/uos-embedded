/**\namespace timer \brief \~english
 * Module "timer" - system time management.
 *
 * Full description of file timer.c.
 */

/**\namespace timer \brief \~russian
 * Модуль "timer" - управление системным временем.
 *
 * Полное описание файла timer.c.
 */
#include <runtime/lib.h>
#include <kernel/uos.h>
#include <kernel/internal.h>
#include <timer/timer.h>
#include <timer/timer-private.h>
#include <timer/timeout.h>

#if defined (ARM_CORTEX_M1) || defined (ARM_CORTEX_M3) || defined (ARM_CORTEX_M4)
extern volatile uint32_t __timer_ticks_uos;
#endif

#ifndef CODE_ISR
#define CODE_ISR
#endif

#ifndef SW_TIMER
#define CODE_TIMER CODE_ISR
#else
#define CODE_TIMER
#endif

#ifndef UOS_ON_TIMER
#define UOS_ON_TIMER(t);
#endif

/**\~english
 * Check that `msec' milliseconds have passed.
 * `Interval' is the time interval, probably rolled over the day.
 *
 * \~russian
 * Проверка, прошло ли указанное количество миллисекунд `msec'.
 * Параметр `interval' содержит интервал времени, возможно, переходящий границу суток.
 */
CODE_TIMER 
bool_t 
interval_greater_or_equal (long interval, long msec)
{
#ifndef TIMER_NO_DAYS
    if (interval < 0)
        interval += TIMER_MSEC_PER_DAY;
    else if (interval >= (long)TIMER_MSEC_PER_DAY)
        interval -= TIMER_MSEC_PER_DAY;
    return (interval >= msec);
#else
    if (interval < 0)
        interval = -interval;
    return (interval >= msec);
#endif
}



/*
 * Timer update function.
 */
CODE_TIMER 
inline void timer_mutex_note(mutex_t* t, unsigned long message){
    mutex_awake (t, (void*) message);
}

#ifndef SW_TIMER
CODE_TIMER 
static inline
#endif
void timer_update (timer_t *t)
{
#if defined (ARM_CORTEX_M1) || defined (ARM_CORTEX_M3) || defined (ARM_CORTEX_M4)
	__timer_ticks_uos++;

	if (__timer_ticks_uos==0)
		__timer_ticks_uos++;
#endif
/*debug_printf ("<ms=%ld> ", t->milliseconds);*/

#ifdef NSEC_TIMER
    const unsigned long interval = t->nsec_per_tick;
#elif defined(USEC_TIMER)
    const unsigned long interval = t->usec_per_tick;
#else
    const unsigned long interval = t->msec_per_tick;
#endif

    unsigned long msec = t->milliseconds;

#ifndef TIMER_NO_DECISEC
    unsigned long nextdec = t->next_decisec;
#else
    unsigned long nextdec = msec;
#endif

    /* Increment current time. */
#ifdef USEC_TIMER
    unsigned long usec = t->usec_in_msec;
    usec += interval;
    while (usec > t->usec_per_tick_msprec){
        usec -= t->usec_per_tick_msprec;
        msec += t->msec_per_tick;
    }
    while (usec > TIMER_USEC_PER_MSEC) {
        msec++;
        usec -= TIMER_USEC_PER_MSEC;
    }
    t->usec_in_msec = usec;
#elif defined(NSEC_TIMER)
    t->nsec_in_msec += t->nsec_per_tick;
    while (t->nsec_in_msec >= TIMER_NSEC_PER_MSEC) {
        msec++;
        t->nsec_in_msec -= TIMER_NSEC_PER_MSEC;
    }
#else
    msec += interval;
#endif //USEC_TIMER

#ifndef TIMER_NO_DAYS
    if (msec >= TIMER_MSEC_PER_DAY) {
        t->days++;
        msec -= TIMER_MSEC_PER_DAY;
        nextdec -= TIMER_MSEC_PER_DAY;
    }
#endif

    t->milliseconds = msec;

#ifndef TIMER_NO_DECISEC
    /* Send signal every 100 msec. */ /* Потенциально не работает, если t->msec_per_tick > 100 */
#if defined(NSEC_TIMER)
    if ((interval >= (TIMER_DECISEC_MS*10000000ul)) &&
#elif defined(USEC_TIMER)
    if ((interval >= (TIMER_DECISEC_MS*1000ul)) &&
#else
    if ((interval >= TIMER_DECISEC_MS) &&
#endif
        (msec >= nextdec)) 
    {
        nextdec += TIMER_DECISEC_MS;
/* debug_printf ("<ms=%lu,nxt=%lu> ", t->milliseconds, t->next_decisec); */
        timer_mutex_note(&t->decisec, msec);
    }
    t->next_decisec = nextdec;
#endif

#ifdef TIMER_TIMEOUTS
    if (! list_is_empty (&t->timeouts)) {
        timeout_t *ut;
        list_iterate (ut, &t->timeouts) {
            long now = ut->cur_time;
            if (now <= 0) {
                if (ut->autoreload == 0){
                    timeout_t *prev_to = (timeout_t *) ut->item.prev;
                    list_unlink (&ut->item);
                    ut = prev_to;
                }
                continue;
            }
            now  -= interval;
            if (now <= 0) {
                if (ut->autoreload > 0){
                    now += ut->interval;
                }
                ut->cur_time = now;

                if (ut->handler)
                    ut->handler(ut, ut->handler_arg);
                if (ut->mutex)
                    timer_mutex_note(ut->mutex, (unsigned long)ut->signal);

                if (ut->autoreload == 0){
                    timeout_t *prev_to = (timeout_t *) ut->item.prev;
                    list_unlink (&ut->item);
                    ut = prev_to;
                }
            }
            else
            ut->cur_time = now;
        }
    }
#endif //USER_TIMERS
#if defined(NSEC_TIMER)
    if (t->nsec_per_tick != t->nsec_per_tick_new) {
        t->nsec_per_tick = t->nsec_per_tick_new;
        hw_set_period(t->khz, t->nsec_per_tick);
    }
#elif defined(USEC_TIMER)
    if (t->usec_per_tick != t->usec_per_tick_new) {
        t->usec_per_tick = t->usec_per_tick_new;
        hw_set_period(t->khz, t->usec_per_tick);
    }
#else
    if (t->msec_per_tick != t->msec_per_tick_new) {
        t->msec_per_tick = t->msec_per_tick_new;
        hw_set_period(t->khz, t->msec_per_tick);
    }
#endif
}

/*
 * Timer interrupt handler.
 */
#ifndef UOS_ON_TIMER
#define UOS_ON_TIMER(t)
#else
CODE_TIMER
__attribute__((weak, noinline))
void uos_on_timer_hook(timer_t *t)
{}
#endif

CODE_TIMER 
bool_t 
timer_handler (timer_t *t)
{
/*debug_printf ("<ms=%ld> ", t->milliseconds);*/

    hw_clear_interrupt ();

    UOS_ON_TIMER(t);
    timer_update (t);

    arch_intr_allow (TIMER_IRQ);

    /* Must signal a lock, for timer_wait().
     * TODO: Optimize timer_delay, keeping a sorted
     * queue of delay values. */
    return 0;
}

/**\~english
 * Return the (real) time in milliseconds since uOS start.
 *
 * \~russian
 * Запрос времени в миллисекундах.
 */
unsigned long
timer_milliseconds (timer_t *t)
{
    unsigned long val;

    mutex_lock (&t->lock);
    val = t->milliseconds;
    mutex_unlock (&t->lock);
    return val;
}

uint64_t
timer_microseconds (timer_t *t)
{
    uint64_t val;

    mutex_lock (&t->lock);
    val = t->milliseconds * 1000;
#if defined(NSEC_TIMER)
    val += t->nsec_in_msec / 1000;
#elif defined(USEC_TIMER)
    val += t->usec_in_msec;
#endif
    mutex_unlock (&t->lock);
    return val;
}

/*
 * Return the (real) time in days and milliseconds since uOS start.
 *
 * \~russian
 * Запрос времени в сутках и миллисекундах.
 */
#ifndef TIMER_NO_DAYS
unsigned int
timer_days (timer_t *t, unsigned long *milliseconds)
{
    unsigned short val;

    if (!milliseconds){
        val = t->days;
    }
    else{
        *milliseconds = t->milliseconds;
        val = t->days;
        while (*milliseconds != t->milliseconds){
            *milliseconds = t->milliseconds;
            val = t->days;
        } 
    }
    return val;
}
#endif

/**\~english
 * Delay the current task by the given time in milliseconds.
 *
 * \~russian
 * Задержка выполнения текущей задачи.
 */
void
timer_delay (timer_t *t, unsigned long msec)
{
    unsigned long t0;

    t0 = t->milliseconds;
    while (! interval_greater_or_equal (t->milliseconds - t0, msec)) {
        mutex_wait (&t->lock);
    }
}

void
timer_delay_ticks (timer_t *t, clock_time_t ticks)
{
    unsigned long t0;

    t0 = t->tick;
    while ((t->tick - t0) < ticks) {
        mutex_wait (&t->lock);
    }
}

/**\~english
 * Check that `msec' milliseconds passed since the `t0' moment.
 *
 * \~russian
 * Проверка временного события.
 */
bool_t
timer_passed (timer_t *t, unsigned long t0, unsigned int msec)
{
    unsigned long now = timer_milliseconds(t);
    return interval_greater_or_equal (now - t0, msec);
}

static inline unsigned long umuldiv1000(unsigned long khz, unsigned long usec_per_tick){
    //res = khz*usec_per_tick /1000
    unsigned long long res = khz>>3;
    res = res * usec_per_tick;
    //const long Nmod = 128*128; 
    //res = res*(Nmod/125);
    //return res/Nmod;
    res = res / 125;
    return res;
} 

unsigned long timer_seconds (timer_t *t){
    unsigned long ms;
    unsigned long days = 0;
#ifndef TIMER_NO_DAYS
    days = timer_days (t, &ms);
#else
    ms = timer_miliseconds(t);
#endif
    unsigned long secs = umuldiv1000(TIMER_MSEC_PER_DAY, days) + umuldiv1000(ms, 1);
    return secs;
}

#if defined(NSEC_TIMER)
/**\~english
 * Nanosecond Timer initialization.
 *
 * \~russian
 * Инициализация наносекундного таймера.
 */
void
timer_init_ns (timer_t *t, unsigned long khz, unsigned long nsec_per_tick)
{
    t->nsec_per_tick = nsec_per_tick;
    t->nsec_per_tick_new = nsec_per_tick;
    t->khz = khz;

#ifndef SW_TIMER
    /* Attach fast handler to timer interrupt. */
    mutex_attach_irq (&t->lock, TIMER_IRQ, (handler_t) timer_handler, t);
    /* Initialize hardware */
    hw_set_period(khz, nsec_per_tick);
#endif // SW_TIMER

#ifdef TIMER_TIMEOUTS
    list_init (&t->timeouts);
#endif
}

#elif defined (USEC_TIMER)

uint64_t
timer_microseconds (timer_t *t)
{
    uint64_t val;

    unsigned long ms;
    unsigned long us;

    do {
    ms = t->milliseconds;
    us = t->usec_in_msec;
    } while (ms != t->milliseconds);
    val = ms * 1000;
    val += us;
    return val;
}

static inline unsigned long timer_period_byus(unsigned long khz, unsigned long usec_per_tick){
    return umuldiv1000(khz, usec_per_tick);
}

/**\~english
 * Nanosecond Timer initialization.
 *
 * \~russian
 * Инициализация наносекудного таймера.
 */
void
timer_init_us (timer_t *t, unsigned long khz, unsigned long usec_per_tick)
{
    t->usec_per_tick = usec_per_tick;
    unsigned tick_ms = usec_per_tick/1000;
    t->msec_per_tick = tick_ms;
    if (tick_ms > 0)
        t->usec_per_tick_msprec = tick_ms*1000;
    else
        t->usec_per_tick_msprec = ~0;
    t->khz = khz;

#ifdef TIMER_TIMEOUTS
    list_init (&t->timeouts);
#endif

#ifndef SW_TIMER
    /* Attach fast handler to timer interrupt. */
    mutex_attach_irq (&t->lock, TIMER_IRQ, (handler_t) timer_handler, t);
    /* Initialize hardware */
    hw_set_period(khz, nsec_per_tick);
#endif // SW_TIMER

#ifdef TIMER_TIMEOUTS
    list_init (&t->timeouts);
#endif
}

#else //USEC_TIMER

/**\~english
 * Create timer task.
 *
 * \~russian
 * Инициализация таймера.
 */
void CODE_ISR
timer_init (timer_t *t, unsigned long khz, small_uint_t msec_per_tick)
{
    t->msec_per_tick = msec_per_tick;
    t->msec_per_tick_new = msec_per_tick;
    t->khz = khz;

#ifndef SW_TIMER
    /* Attach fast handler to timer interrupt. */
    mutex_attach_irq (&t->lock, TIMER_IRQ, (handler_t) timer_handler, t);
    /* Initialize the hardware. */
    hw_set_period(khz, msec_per_tick);
#endif // SW_TIMER

#ifdef TIMER_TIMEOUTS
    list_init (&t->timeouts);
#endif
}
#endif // NSEC_TIMER, USEC_TIMER

void timer_set_period(timer_t *t, small_uint_t msec_per_tick)
{
#if defined(NSEC_TIMER)
    t->nsec_per_tick_new = msec_per_tick * 1000000;
#elif defined(USEC_TIMER)
    t->usec_per_tick_new = msec_per_tick * 1000;
#else
    t->msec_per_tick_new = msec_per_tick;
#endif
}

void timer_set_period_us(timer_t *t, unsigned long usec_per_tick)
{
#if defined(NSEC_TIMER)
    t->nsec_per_tick_new = usec_per_tick * 1000;
#elif defined(USEC_TIMER)
    t->usec_per_tick_new = usec_per_tick;
#else
    t->msec_per_tick_new = usec_per_tick / 1000;
#endif
}

void timer_set_period_ns(timer_t *t, unsigned long nsec_per_tick)
{
#if defined(NSEC_TIMER)
    t->nsec_per_tick_new = nsec_per_tick;
#elif defined(USEC_TIMER)
    t->usec_per_tick_new = nsec_per_tick / 1000;
#else
    t->msec_per_tick_new = nsec_per_tick / 1000000;
#endif
}

