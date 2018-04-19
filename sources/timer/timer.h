#ifndef _TIMER_H_
#define _TIMER_H_

#ifdef __cplusplus
extern "C" {
#endif

/**\~english
 * Number of milliseconds per day.
 *
 * \~russian
 * Количество миллисекунд в дне.
 */
#define TIMER_MSEC_PER_DAY  (24UL*60*60*1000)
#define TIMER_USEC_PER_MSEC (1000UL)
#define TIMER_NSEC_PER_MSEC (1000000UL)

struct _timer_t {
    mutex_t lock;
    mutex_t decisec;        /* every 0.1 second is signalled here */
    unsigned long khz;      /* reference clock */
    
#if defined(NSEC_TIMER)
    unsigned long nsec_per_tick;
    unsigned long nsec_per_tick_new;
    unsigned long nsec_in_msec;
#elif defined(USEC_TIMER)
    unsigned long usec_per_tick;
    unsigned long usec_per_tick_new;
    unsigned long usec_in_msec;
#else
    small_uint_t msec_per_tick;
    small_uint_t msec_per_tick_new;
#endif

#if PIC32MX
    unsigned compare_step;
#endif
    unsigned long milliseconds; /* real time counter */
    unsigned long next_decisec; /* when next decisecond must be signalled */
    unsigned int days;      /* days counter */

#ifdef TIMER_TIMEOUTS
    list_t timeouts;
#endif
};

/**\~english
 * Data structure of a timer driver.
 *
 * \~russian
 * Структура данных для драйвера таймера.
 */
typedef struct _timer_t timer_t;


#if defined(NSEC_TIMER)
// Наносекундный таймер
void timer_init_ns (timer_t *t, unsigned long khz, unsigned long nsec_per_tick);
#elif defined(USEC_TIMER)
// Микросекудный таймер
void timer_init_us (timer_t *t, unsigned long khz, unsigned long usec_per_tick);
#else
void timer_init (timer_t *t, unsigned long khz, small_uint_t msec_per_tick);
#endif

#ifdef SW_TIMER
void timer_update (timer_t *t);
#endif

/* Delay current task. */
void timer_delay (timer_t *t, unsigned long msec);

/* Query real time. */
unsigned long timer_milliseconds (timer_t *t);
uint64_t timer_microseconds (timer_t *t);
unsigned int timer_days (timer_t *t, unsigned long *milliseconds);
bool_t timer_passed (timer_t *t, unsigned long t0, unsigned int msec);
bool_t interval_greater_or_equal (long interval, long msec);

void timer_set_period(timer_t *t, small_uint_t msec_per_tick);
void timer_set_period_us(timer_t *t, unsigned long usec_per_tick);
void timer_set_period_ns(timer_t *t, unsigned long nsec_per_tick);

#ifdef __cplusplus
}
#endif

#endif /* _TIMER_H_ */
