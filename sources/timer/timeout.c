#include <runtime/lib.h>
#include <kernel/uos.h>
#include <kernel/internal.h>
#include <timer/timer.h>
#include <timer/timeout.h>


void timeout_clear(timeout_t *to){
    memset(to, 0, sizeof(timeout_t));
    list_init(&to->item);
}

#ifdef TIMER_TIMEOUTS

void timeout_init (timeout_t *to, timer_t *timer, mutex_t *mutex)
{
    timeout_clear(to);
    to->mutex = mutex;
    to->timer = timer;
    timeout_add(timer, to);
}

void timeout_add (timer_t *t,       timeout_t *ut)
{
    if (t != 0){
        ut->timer = t;
    }
    assert(ut->timer != 0);
    mutex_lock (&t->lock);
    list_append (&(ut->timer->timeouts), &ut->item);
    mutex_unlock (&t->lock);
}

void timeout_start (timeout_t *to)
{
    while (to->cur_time != (long)to->interval)
        to->cur_time = (long)to->interval;
    if (list_is_empty(&to->item))
        timeout_add(to->timer, to);
}

void timeout_remove (timeout_t *ut){
    timer_t* t = ut->timer;
    if (t == 0)
        return;
    if (list_is_empty(&ut->item))
        return;
    mutex_lock (&t->lock);
    list_unlink (&ut->item);
    mutex_unlock (&t->lock);
}

#endif

/**\~russian
 * запускает одноразовый таймер с текущего момента.
 * xsec_time - время задержки события таймера
 * вызов   user_timer_arm(, 0) - останавливает таймер, аналогичен user_timer_stop
 * */
#ifdef USEC_TIMER
void timeout_arm_us  (timeout_t *ut, timeout_time_t interval)
#elif defined(NSEC_TIMER)
void timeout_arm_ns  (timeout_t *ut, timeout_time_t interval)
#else
void timeout_arm  (timeout_t *ut, timeout_time_t interval)
#endif
{
    arch_state_t x = arch_intr_off();
    ut->interval = interval;
    ut->cur_time = interval;
    //ut->autoreload = tsLoadOnce;
    arch_intr_restore (x);
}

/**\~russian
 * запускает одноразовый таймер с момента предыдущего события.
 * xsec_time - время задержки события таймера
 * вызов   user_timer_arm(, 0) - останавливает таймер, аналогичен user_timer_stop
 * !!! если вновь выставленное событие уже просрочено - возвращается true
 * return false - если запущеное событие еще ожидается
 * */
#ifdef USEC_TIMER
bool_t timeout_rearm_us  (timeout_t *ut, timeout_time_t interval)
#elif defined(NSEC_TIMER)
bool_t timeout_rearm_ns  (timeout_t *ut, timeout_time_t interval)
#else
bool_t timeout_rearm  (timeout_t *ut, timeout_time_t interval)
#endif
{
    bool_t res;
    arch_state_t x = arch_intr_off();
    ut->interval = interval;
    ut->cur_time += interval;
    //ut->autoreload = tsLoadOnce;
    res = ut->cur_time <=0;
    arch_intr_restore (x);
    return res;
}


/**\~russian
 * перезапускает периодический таймер с момента предыдущего события.
 * xsec_interval - новый период событий таймера
 * вызов   user_timer_restart_interval(, 0) - останавливает таймер, аналогичен user_timer_stop
 * */
#ifdef USEC_TIMER
void timeout_restart_interval_us (timeout_t *ut, timeout_time_t interval)
#else
void timeout_restart_interval (timeout_t *ut, timeout_time_t interval)
#endif
{
    arch_state_t x = arch_intr_off();
    if (ut->cur_time > 0){
        long delta = ut->interval - interval;
        ut->cur_time += delta;
    }
    else{
        ut->cur_time += interval;
    }
    ut->interval = interval;
    ut->autoreload = tsLoadAuto;
    /*
    if (list_is_empty(to->item)){
        assert(to->timer != 0);
        list_append (&to->timer->timeouts, &to->item);
    }
    */
    arch_intr_restore (x);
}


#ifdef USER_TIMERS

#ifdef USEC_TIMER
void user_timer_init(user_timer_t *ut, usertimer_time_t msec_per_tick)
{
    user_timer_init_us(ut, msec_per_tick*USER_TIMER_MS);
}

void user_timer_init_us (user_timer_t *ut, usertimer_time_t usec_per_tick)
{
    timeout_t* to = &(ut->to);
    timeout_clear(to);
    timeout_set_mutex(to, &(ut->lock), to);
    timeout_set_value_us(&(ut->to), usec_per_tick);
    timeout_set_autoreload(to, tsLoadOnce);
}
#else
void user_timer_init (user_timer_t *ut, usertimer_time_t usec_per_tick)
{
    timeout_t* to = &(ut->to);
    timeout_clear(to);
    timeout_set_mutex(to, &(ut->lock), to);
    timeout_set_value(&(ut->to), usec_per_tick);
    timeout_set_autoreload(to, tsLoadOnce);
}
#endif

#endif //USER_TIMERS
