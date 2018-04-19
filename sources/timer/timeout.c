#include <runtime/lib.h>
#include <kernel/uos.h>
#include <kernel/internal.h>
#include <timer/timer.h>
#include <timer/timeout.h>


void timeout_init (timeout_t *to, timer_t *timer, mutex_t *mutex)
{
    assert(timer != 0);
    assert(mutex != 0);
    
    memset(to, 0, sizeof(timeout_t));
    
    list_init(&to->item);
    to->mutex = mutex;
    to->timer = timer;
}

void timeout_set_signal (timeout_t *to, void *signal)
{
    to->signal = signal;
}

void timeout_set_value (timeout_t *to, unsigned long interval_msec)
{
#if defined(NSEC_TIMER)
    to->interval = (unsigned long long) interval_msec * 1000000UL;
#elif defined(USEC_TIMER)
    to->interval = (unsigned long long) interval_msec * 1000UL;
#else
    to->interval = interval_msec;
#endif
}

void timeout_set_value_us (timeout_t *to, unsigned long long interval_usec)
{
#if defined(NSEC_TIMER)
    to->interval = interval_usec * 1000;
#elif defined(USEC_TIMER)
    to->interval = interval_usec;
#else
    to->interval = interval_usec / 1000;
#endif
}

void timeout_set_value_ns (timeout_t *to, unsigned long long interval_nsec)
{
#if defined(NSEC_TIMER)
    to->interval = interval_nsec;
#elif defined(USEC_TIMER)
    to->interval = interval_nsec / 1000;
#else
    to->interval = interval_nsec / 1000000;
#endif
}

void timeout_set_autoreload (timeout_t *to, int autoreload)
{
    to->autoreload = autoreload;
}

void timeout_set_handler (timeout_t *to, timeout_handler handler, void *arg)
{
    to->handler = handler;
    to->handler_arg = arg;
}

void timeout_start (timeout_t *to)
{
    mutex_lock(&to->timer->lock);
    list_append(&to->timer->timeouts, &to->item);
    to->cur_time = to->interval;
    mutex_unlock(&to->timer->lock);
}

void timeout_stop (timeout_t *to)
{
    mutex_lock(&to->timer->lock);
    list_unlink(&to->item);
    mutex_unlock(&to->timer->lock);
}


