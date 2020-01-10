#ifndef _UOS_TIMER_H_
#define _UOS_TIMER_H_

/*	UTF8 ru  */

#include <kernel/uos.h>

#ifndef INLINE
#define INLINE static inline
#endif

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

/**\~russian
 * макро SW_TIMER отключает инициализацию обработчика прерывания системного таймера. в этом случае настраивает и запускает таймер
 * пользовательский код. обновление таймера производится вызовом timer_update
 * */
//#define SW_TIMER 1

/**\~russian
 * макро USER_TIMERS включает в таймер функционал множественных пользовательских таймеров.
 * !!! DEPRECATED. этот функционал реализуется теперь таймаутами см. <timer/timeout.h>
 * см. функции user_timer_XXX
 * */
//#define USER_TIMERS 1

/**\~russian
 * макро TIMER_TIMEOUTS включает в таймер функционал множественных пользовательских таймаутов.
 * см. функции <timer/timeout.h>:timeout_XXX
 * */
//#define TIMER_TIMEOUTS 1
#if defined(USER_TIMERS) && !defined(TIMER_TIMEOUTS)
#define TIMER_TIMEOUTS 1
#endif

/**\~russian
 * макро USEC_TIMER дает возможность использовать прецизионный таймер сразрешением в [us]. см. timer_init_us
 * */
//#define USEC_TIMER 1

/**\~russian
 * макро TIMER_NO_DAYS опускает реализацию _timer_t.days - экономит время обработчика прерывания
 *  используется модулями: smnp
 * */
//#define TIMER_NO_DAYS 1 

/**\~russian
 * макро TIMER_NO_DECISEC опускает реализацию _timer_t.decisec - экономит время обработчика прерывания
 *  , это поле требуется для протокола TCP!!!
 * */
//#define TIMER_NO_DECISEC 1 

/**\~russian
 * макро TIMER_DECISEC_MS задает период редкого таймера _timer_t.decisec
 * */
#ifndef TIMER_DECISEC_MS
#define TIMER_DECISEC_MS 100
#endif


typedef small_uint_t clock_time_t;
#ifdef UINT_MAX
#define TIME_MAX     UINT_MAX
#else
#define TIME_MAX     ((clock_time_t)(-1))
#endif

#define TIME_INFINITE (TIME_MAX>>1)

struct _timer_t {
    mutex_t lock;
#ifndef TIMER_NO_DECISEC
    mutex_t decisec;        /* every 0.1 second is signalled here */
    volatile unsigned long next_decisec; /* when next decisecond must be signalled */
#endif
    volatile clock_time_t tick;      //this is timer stamp increments on every tick
    unsigned long khz;      /* reference clock */

#if defined(NSEC_TIMER)
    unsigned long nsec_per_tick;
    unsigned long nsec_per_tick_new;
    volatile unsigned long nsec_in_msec;
    unsigned long nsec_per_tick_msprec;
#elif defined(USEC_TIMER)
    unsigned long usec_per_tick;
    unsigned long usec_per_tick_new;
    volatile unsigned long usec_in_msec;
    small_uint_t usec_per_tick_msprec;
#else
    small_uint_t msec_per_tick;
    small_uint_t msec_per_tick_new;
#endif
    small_uint_t msec_per_tick;
    volatile unsigned long milliseconds; /* real time counter */

#if PIC32MX
    unsigned compare_step;
#endif

#ifndef TIMER_NO_DAYS
    volatile unsigned int days;      /* days counter */
#endif

#ifdef TIMER_TIMEOUTS
    list_t timeouts;
#elif defined(USER_TIMERS)
    list_t user_timers;
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
INLINE 
void timer_init (timer_t *t, unsigned long khz, small_uint_t msec_per_tick){
    timer_init_ns (t, khz, msec_per_tick*TIMER_NSEC_PER_MSEC);
};
#elif defined(USEC_TIMER)
// Микросекудный таймер. Поддерживается пока только для контроллеров
//  миландровских, MIPS32
void timer_init_us (timer_t *t, unsigned long khz, unsigned long usec_per_tick);
INLINE 
void timer_init (timer_t *t, unsigned long khz, small_uint_t msec_per_tick){
    timer_init_us (t, khz, msec_per_tick*TIMER_USEC_PER_MSEC);
};
#else
void timer_init (timer_t *t, unsigned long khz, small_uint_t msec_per_tick);
#endif

#ifdef SW_TIMER
void timer_update (timer_t *t);
#endif

/* Delay current task. */
void timer_delay (timer_t *t, unsigned long msec);

/**\~english
 * Return the (real) time in milliseconds since uOS start.
 *
 * \~russian
 * Запрос времени в миллисекундах.
 */
INLINE unsigned long timer_milliseconds (timer_t *t){
    return t->milliseconds;
};

#ifdef USEC_TIMER
uint64_t timer_microseconds (timer_t *t);
#else
INLINE
uint64_t timer_microseconds (timer_t *t){
    return t->milliseconds*1000ULL;
}
#endif

unsigned long timer_seconds (timer_t *t);

/**\~english
 * Check that `msec' milliseconds passed since the `t0' moment.
 *
 * \~russian
 * Проверка временного события.
 */
bool_t timer_passed (timer_t *t, unsigned long t0, unsigned int msec);

#ifndef TIMER_NO_DAYS
unsigned int timer_days (timer_t *t, unsigned long *milliseconds);
#endif

/**\~english
 * Check that `msec' milliseconds have passed.
 * `Interval' is the time interval, probably rolled over the day.
 *
 * \~russian
 * Проверка, прошло ли указанное количество миллисекунд `msec'.
 * Параметр `interval' содержит интервал времени, возможно, переходящий границу суток.
 */
bool_t interval_greater_or_equal (long interval, long msec);

void timer_set_period(timer_t *t, small_uint_t msec_per_tick);
void timer_set_period_us(timer_t *t, unsigned long usec_per_tick);
void timer_set_period_ns(timer_t *t, unsigned long nsec_per_tick);


void timer_delay_ticks (timer_t *t, clock_time_t ticks);

/**\~english
 * Return the (real) time in ticks since uOS start.
 *
 * \~russian
 * Запрос времени в тиках.
 */
INLINE unsigned long timer_tick(timer_t *t){
    return t->tick;
};

/**\~english
 * Check that `ticks' passed since the `t0' moment.
 *
 * \~russian
 * Проверка временного события.
 */
INLINE bool_t timer_passed_tick(timer_t *t, clock_time_t t0, clock_time_t ticks)
{
    unsigned long now = timer_tick(t);
    return (now - t0) > ticks;
}



#ifdef USER_TIMERS
#include <timer/user_timer.h>
#endif

#ifdef __cplusplus
}
#endif

#endif /* _TIMER_H_ */
