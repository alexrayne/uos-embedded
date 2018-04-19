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

/**\~english
 * Check that `msec' milliseconds have passed.
 * `Interval' is the time interval, probably rolled over the day.
 *
 * \~russian
 * Проверка, прошло ли указанное количество миллисекунд `msec'.
 * Параметр `interval' содержит интервал времени, возможно, переходящий границу суток.
 */
small_int_t
interval_greater_or_equal (long interval, long msec)
{
    if (interval < 0)
        interval += TIMER_MSEC_PER_DAY;
    else if (interval >= TIMER_MSEC_PER_DAY)
        interval -= TIMER_MSEC_PER_DAY;
    return (interval >= msec);
}

/*
 * Timer update function.
 */
#ifndef SW_TIMER
static inline
#endif
void timer_update (timer_t *t)
{
#ifdef SW_TIMER
	mutex_activate (&t->lock,
		(void*) (size_t) t->milliseconds);
#endif

#if defined (ARM_CORTEX_M1) || defined (ARM_CORTEX_M3) || defined (ARM_CORTEX_M4)
	__timer_ticks_uos++;

	if (__timer_ticks_uos==0)
		__timer_ticks_uos++;

#endif

    /* Increment current time. */
#if defined(NSEC_TIMER)
    t->nsec_in_msec += t->nsec_per_tick;
    while (t->nsec_in_msec >= TIMER_NSEC_PER_MSEC) {
        t->milliseconds++;
        t->nsec_in_msec -= TIMER_NSEC_PER_MSEC;
    }
#elif defined(USEC_TIMER)
    t->usec_in_msec += t->usec_per_tick;
    while (t->usec_in_msec > TIMER_USEC_PER_MSEC) {
        t->milliseconds++;
        t->usec_in_msec -= TIMER_USEC_PER_MSEC;
    }
#else
    t->milliseconds += t->msec_per_tick;
#endif

    if (t->milliseconds >= TIMER_MSEC_PER_DAY) {
        ++t->days;
        t->milliseconds -= TIMER_MSEC_PER_DAY;
        t->next_decisec -= TIMER_MSEC_PER_DAY;
    }

    /* Send signal every 100 msec. */
#if defined(NSEC_TIMER)
    if (t->nsec_per_tick / 1000000 <= 100 &&
#elif defined(USEC_TIMER)
    if (t->usec_per_tick / 1000 <= 100 &&
#else
    if (t->msec_per_tick <= 100 &&
#endif
        t->milliseconds >= t->next_decisec) {
        t->next_decisec += 100;
/*debug_printf ("<ms=%lu,nxt=%lu> ", t->milliseconds, t->next_decisec);*/
        if (! list_is_empty (&t->decisec.waiters) ||
            ! list_is_empty (&t->decisec.groups)) {
            mutex_activate (&t->decisec,
                (void*) (size_t) t->milliseconds);
        }
    }
    
#ifdef TIMER_TIMEOUTS
    if (! list_is_empty (&t->timeouts)) {
        timeout_t *to;
        list_iterate (to, &t->timeouts) {
#if defined(NSEC_TIMER)
            to->cur_time -= t->nsec_per_tick;
#elif defined(USEC_TIMER)
            to->cur_time -= t->usec_per_tick;
#else
            to->cur_time -= t->msec_per_tick;
#endif
            if (to->cur_time <= 0) {
                if (to->handler)
                    to->handler(to, to->handler_arg);
                if (! list_is_empty (&to->mutex->waiters) ||
                        ! list_is_empty (&to->mutex->groups)) {
                    mutex_activate (to->mutex, to->signal);
                }
                if (to->autoreload) {
                    to->cur_time += to->interval;
                } else {
                    timeout_t *prev_to = (timeout_t *) to->item.prev;
                    list_unlink (&to->item);
                    to = prev_to;
                }
            }
        }
    }
#endif

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
bool_t
timer_handler (timer_t *t)
{
/*debug_printf ("<ms=%ld> ", t->milliseconds);*/

    hw_clear_interrupt ();

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

/**\~english
 * Return the (real) time in days and milliseconds since uOS start.
 *
 * \~russian
 * Запрос времени в сутках и миллисекундах.
 */
unsigned int
timer_days (timer_t *t, unsigned long *milliseconds)
{
    unsigned short val;

    mutex_lock (&t->lock);
    if (milliseconds)
        *milliseconds = t->milliseconds;
    val = t->days;
    mutex_unlock (&t->lock);
    return val;
}

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

    mutex_lock (&t->lock);
    t0 = t->milliseconds;
    while (! interval_greater_or_equal (t->milliseconds - t0, msec)) {
        mutex_wait (&t->lock);
    }
    mutex_unlock (&t->lock);
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
    unsigned long now;

    mutex_lock (&t->lock);
    now = t->milliseconds;
    mutex_unlock (&t->lock);

    return interval_greater_or_equal (now - t0, msec);
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

#elif defined(USEC_TIMER)
/**\~english
 * Microsecond Timer initialization.
 *
 * \~russian
 * Инициализация микросекудного таймера.
 */
void
timer_init_us (timer_t *t, unsigned long khz, unsigned long usec_per_tick)
{
    t->usec_per_tick = usec_per_tick;
    t->usec_per_tick_new = usec_per_tick;
    t->khz = khz;

#ifndef SW_TIMER
    /* Attach fast handler to timer interrupt. */
    mutex_attach_irq (&t->lock, TIMER_IRQ, (handler_t) timer_handler, t);
    /* Initialize hardware */
    hw_set_period(khz, usec_per_tick);
#endif // SW_TIMER

#ifdef TIMER_TIMEOUTS
    list_init (&t->timeouts);
#endif
}
#else

/**\~english
 * Create timer task.
 *
 * \~russian
 * Инициализация таймера.
 */
void
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

