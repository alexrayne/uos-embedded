#ifndef _UOS_TIMEOUT_H_
#define _UOS_TIMEOUT_H_

/*  UTF8 ru  */

/**\~russian
 * !!! чтобы функционал таймаутов был активен на объектах таймеров надо его подключить макро TIMER_TIMEOUTS
 * см. <timer/timer.h>
 * */

#include <kernel/uos.h>
#include <timer/timer.h>



//                              Таймауты таймера
//
//     Таймауты таймера дополняют возможности системного таймера uOS (timer_t).
// Основная идея таймаута заключается в том, что по истечении заданного
// интервала времени на мьютекс, связанный с параметром, посылается сигнал,
// позволяющий "снять" задачу с точки ожидания (mutex_wait) до наступления
// ожидаемого события.
//
//     Типовые варианты использования таймаутов:
//
// ВАРИАНТ 1. 
//    Снятие задачи с точки ожидания, если ожидаемое событие не наступило по
// прошествии заданного интервала времени.
//    Предположим, что задача ожидает сигнала (например, от прерывания) на 
// некотором мьютексе. Если сигнал не пришёл в течении 100 мс, то задача должна
// сняться с точки ожидания и обработать ошибку: таймаут.
//    Приблизительный код для реализации описанного поведения приведён ниже:
//      
//      mutex_t mutex;
//      timer_t timer;
//      timeout_t timeout;
//
//      ...
//
//      // Инициализация (например, в функции uos_init() или начале задачи).
//      timeout_init (&timeout, &timer, &mutex);
// 
//      ...
//
//      // Использование:
//      const unsigned TIMEOUT_SIGNAL = 0xFFFFFFFF; // Здесь любое число, 
//                                                  // отличное от 0.
//      timeout_set_value (&timeout, 100);          // Таймаут через 100 мс.
//
//      // Устанавливаем значение сигнала, который придёт на мьютекс в случае
//      // таймаута:
//      timeout_set_signal (&timeout, (void *) TIMEOUT_SIGNAL);
//
//      // Занимаем мьютекс, чтобы не было разрыва между запуском таймаута и 
//      // началом ожидания. Если мьютекс уже занят, делать этого не надо.
//      mutex_lock (&mutex);
//
//      // Запускаем таймер таймаута
//      timeout_start (&timeout);
//
//      // Ожидаем события на мьютексе
//      if (mutex_wait (&mutex) == (void *) TIMEOUT_SIGNAL) {
//          // Произошёл таймаут - далее его обработка
//          ...
//      } else {
//          // Произошло ожидаемое событие. Функция mutex_wait возвращает 
//          // значение, переданное во втором параметре функции mutex_signal()
//          // (mutex_activate()). Как правило, это ноль.
//          
//          timeout_stop (&timeout);
//          // Далее обработка события:
//          ...
//      }
//      
//      mutex_unlock (&mutex);
//
//    Недостатком реализации таймаутов является то, что если задача была 
// вытеснена в момент между вызовами функций timeout_start() и mutex_wait(),
// то время на ожидание события сокращается на время вытеснения. Это
// необходимо учитывать при разработке. Но это не большое ограничение, так
// как подразумевается, что интервал таймаута существенно превышает типичное
// время ожидания события.
//    Кроме того, необходимо учитывать, что объект таймаута отсчитывает
// выделенное ему время по таймеру, указанному в параметре функции 
// timeout_init(). Этот таймер имеет заданный период следования тиков (в
// параметре функции timer_init()). Точность таймаута всегда не выше этого
// периода, и действительное время таймаута всегда кратно этому периоду с 
// округлением в большую сторону при необходимости.
//
// ВАРИАНТ 2.
//    Периодические сигналы на указанный мьютекс.
//    На обычном таймере в общем случае нельзя сделать фиксированную длину цикла
// активации задачи. Например, рассмотрим следующее тело задачи:
//
//      void task (void *arg)
//      {
//          for (;;) {
//              do_work();
//              timer_delay(&timer, 10);
//          }
//      }
//
//    Длина цикла этой задачи зависит от времени выполнения функции do_work().
// Длина цикла = время(do_work) + 10 мс. Если время выполнения do_work меньше
// периода тиков таймера timer, то длительность всего цикла задачи может
// получиться равной 10 мс, но данный способ слишком ненадёжен, если нужно
// обеспечить строго 10-миллисекундный цикл.
//    Для решения этой проблемы можно использовать таймауты с установленным
// параметров autoreload. Пример кода:
//
//      mutex_t mutex;
//      timer_t timer;
//      timeout_t timeout;
//
//      ...
//
//      // Инициализация (например, в функции uos_init() или начале задачи).
//      timer_init (&timer, 2);                     // Период системного таймера
//                                                  // должен быть меньше
//                                                  // времени таймаута.
//      timeout_init (&timeout, &timer, &mutex);
//      timeout_set_value (&timeout, 10);           // Длина периода 10 мс.
//                                                  // Чтобы периоды были
//                                                  // равными, нужно, чтобы
//                                                  // длина периода была кратна
//                                                  // периоду таймера.
//      timeout_set_autoreload (&timeout, 1);       // (!)Периодический таймаут.
//
//      ...
//
//      // Использование:
//      void task (void *arg)
//      {
//          mutex_lock (&mutex);
//          timeout_start (&timeout);
//          for (;;) {
//              mutex_wait (&mutex);
//              do_work();
//          }
//      }
//
// ВАРИАНТ 3.
//    Реализация функции задержки.
//    Имеет смысл использовать объекты таймаутов данным образом только для
// реализации микросекундных задержек. Это будет работать только если включено
// глобальное макроопределение USEC_TIMER, таймер проинициализирован с помощью
// функции timer_init_us(). Задержка задаётся с помощью timeout_set_value_us().
//
//    Дополнительно объекты таймаутов имеют возможность подключения функции-
// обработчика.

// !!! таймаут с текущим значением счета <=0 несчитает. тоесть если форсировать таймаут в 0,
//      он станет неактивным, и перестанет считать. при этом таймаут типа tsLoadRemove
//      сам выпилится из списка таймеров.
// timeout_start/stop - соответственно запускают, тормозят таймер


#ifdef __cplusplus
extern "C" {
#endif

typedef struct _timeout_t timeout_t;

// Прототип функции-обработчика таймаута.
typedef void (* timeout_handler)(timeout_t *to, void *arg);
typedef unsigned long timeout_time_t;


struct _timeout_t
{
    list_t item;
    
    timer_t *timer;
    
    mutex_t *mutex;
    void *signal;
    
    timeout_handler handler;
    void *handler_arg;
    
    timeout_time_t interval;
    volatile long cur_time;
    
    int autoreload;
};

void timeout_clear(timeout_t *to);

#ifdef TIMER_TIMEOUTS

/**\~russian
 * создает чистый таймаут и сразу подключает его к таймеру timer
 * */
//
// Инициализация объекта таймаута.
//
// to       - объект таймаута;
// timer    - объект таймера, по которому отсчитывается таймаут;
// mutex    - мьютекс, на который придёт сигнал в случае срабатывания таймаута.
//
void timeout_init (timeout_t *to, timer_t *timer, mutex_t *mutex);

/**\~russian
 * подключает/отключает таймаут к таймеру
 * */
void timeout_add (timer_t *t,       timeout_t *ut);
void timeout_remove (timeout_t *ut);

//
// Запуск отсчёта таймаута.
//
// to               - объект таймаута.
//
void timeout_start (timeout_t *to);

//
// Останов отсчёта таймаута.
//
// to               - объект таймаута.
//
INLINE
void timeout_stop (timeout_t *to)
{
    timeout_remove(to);
}

#endif //TIMER_TIMEOUTS

INLINE
void timeout_wait (timeout_t *ut)
{
    mutex_wait (ut->mutex);
}

/**\~russian
 * перезапускает периодический таймер с текущего момента.
 * этот функционал не подключает таймаут к таймеру, он не расчитан на работу с режимом tsLoadRemove
 * */
INLINE
void timeout_restart (timeout_t *to)
{
    while (to->cur_time != (long)to->interval)
        to->cur_time = (long)to->interval;
}

/**\~russian
 * останавливает активность таймера.
 * этот функционал не отключает таймаут от таймера.
 * в режимом tsLoadRemove таймер отключает прерваный таймаут в ближайший отсчет.
 * */
INLINE
void timeout_break (timeout_t *to)
{
    while (to->cur_time != 0)
        to->cur_time = 0;
}

//
// Установка сигнала таймаута
//
// to       - объект таймаута;
// signal   - значение сигнала, который придёт на мьютекс при срабатывании 
//            таймаута.
//
INLINE
void timeout_set_signal (timeout_t *to, void *signal){
    to->signal = signal;
}

INLINE
void timeout_set_mutex (timeout_t *to, mutex_t *mutex, void *signal){
    to->mutex = mutex;
    to->signal = signal;
}

enum timeout_autoreload_style{
    //* stops timeout after arming, but leave it in timer check-list with inactive state
     tsLoadOnce      = -1
        //*remove timeout from timer check-list after arming
        //*this is default timeout behaviour
    , tsLoadRemove    = 0
    //* automaticaly restart timeout after arming
    , tsLoadAuto      = 1
};
typedef int timeout_style;

//
// Установка параметра автоматического перезапуска таймаута.
//
// to               - объект таймаута;
// autoreload       - см timeout_atoreload_style
//
INLINE
void timeout_set_autoreload (timeout_t *to, timeout_style autoreload) {
    to->autoreload = autoreload;
}

//
// Установка функции-обработчика таймаута.
//
// to               - объект таймаута;
// handler          - функция-обработчик;
// arg              - значение параметра функции-обработчика.
//
INLINE
void timeout_set_handler (timeout_t *to, timeout_handler handler, void *arg) {
    to->handler = handler;
    to->handler_arg = arg;
}

#ifdef USEC_TIMER
#define TIMEOUT_TIMER_MS   (1000UL)

//
// Установка периода таймаута в микросекундах.
//
// to               - объект таймаута;
// interval_usec    - период в мкс.
//
INLINE
void timeout_set_value_us (timeout_t *to, unsigned long interval_usec){
    to->interval = interval_usec;
}

//
// Установка периода таймаута в миллисекундах.
//
// to               - объект таймаута;
// interval_msec    - период в мс.
//
INLINE
void timeout_set_value (timeout_t *to, unsigned long interval_msec){
    timeout_set_value_us(to, interval_msec*TIMEOUT_TIMER_MS);
}

/**\~russian
 * перезапускает периодический таймер с момента предыдущего события.
 * xsec_interval - новый период событий таймера
 * вызов   user_timer_restart_interval(, 0) - останавливает таймер, аналогичен user_timer_stop
 * */
void timeout_restart_interval_us (timeout_t *ut, timeout_time_t interval_us);
INLINE
void timeout_restart_interval (timeout_t *to, unsigned long interval_msec){
    timeout_restart_interval_us(to, interval_msec*TIMEOUT_TIMER_MS);
}

/**\~russian
 * запускает одноразовый таймер с текущего момента.
 * xsec_time - время задержки события таймера
 * вызов   user_timer_arm(, 0) - останавливает таймер, аналогичен user_timer_stop
 * */
void timeout_arm_us  (timeout_t *ut, timeout_time_t interval_us);
INLINE
void timeout_arm (timeout_t *to, unsigned long interval_msec){
    timeout_arm_us(to, interval_msec*TIMEOUT_TIMER_MS);
}

//* тоже user_timer_arm_us, но не защищен от конкурентности,
//  можно вызывать из прерываиий, если заведомо нет конкуренции
INLINE
void timeout_arm_us_nomt (timeout_t *ut, timeout_time_t interval){
    ut->interval = interval;
    ut->cur_time = interval;
    //ut->autoreload = tsLoadOnce;
}
INLINE
void timeout_arm_nomt (timeout_t *to, unsigned long interval_msec){
    timeout_arm_us_nomt(to, interval_msec*TIMEOUT_TIMER_MS);
}

/**\~russian
 * запускает одноразовый таймер с момента предыдущего события.
 * xsec_time - время задержки события таймера
 * вызов   user_timer_arm(, 0) - останавливает таймер, аналогичен user_timer_stop
 * !!! если вновь выставленное событие уже просрочено - возвращается true
 * return false - если запущеное событие еще ожидается
 * */
bool_t timeout_rearm_us  (timeout_t *ut, timeout_time_t interval);
INLINE
bool_t timeout_rearm (timeout_t *to, unsigned long interval_msec){
    return timeout_rearm_us(to, interval_msec*TIMEOUT_TIMER_MS);
}

INLINE
bool_t timeout_rearm_us_nomt (timeout_t *ut, timeout_time_t interval){
    ut->interval = interval;
    ut->cur_time += interval;
    //ut->autoreload = tsLoadOnce;
    return ut->cur_time <=0;
}
INLINE
bool_t timeout_rearm_nomt (timeout_t *to, unsigned long interval_msec){
    return timeout_rearm_us_nomt(to, interval_msec*TIMEOUT_TIMER_MS);
}

#elif defined(NSEC_TIMER)
#define TIMEOUT_TIMER_MS   (1000000UL)

// TODO
//
// Установка периода таймаута в наносекундах.
//
// to               - объект таймаута;
// interval_nsec    - период в нс.
//
INLINE
void timeout_set_value_ns (timeout_t *to, unsigned long long interval_nsec){
    to->interval = interval_usec;
}


//
// Установка периода таймаута в миллисекундах.
//
// to               - объект таймаута;
// interval_msec    - период в мс.
//
INLINE
void timeout_set_value (timeout_t *to, unsigned long interval_msec){
    timeout_set_value_ns(to, interval_msec*TIMEOUT_TIMER_MS);
}

#else //USEC_TIMER
#define TIMEOUT_TIMER_MS   (1UL)

//
// Установка периода таймаута в миллисекундах.
//
// to               - объект таймаута;
// interval_msec    - период в мс.
//
INLINE
void timeout_set_value (timeout_t *to, unsigned long interval_msec){
    to->interval = interval_msec;
}

/**\~russian
 * перезапускает периодический таймер с момента предыдущего события.
 * xsec_interval - новый период событий таймера
 * вызов   user_timer_restart_interval(, 0) - останавливает таймер, аналогичен user_timer_stop
 * */
void timeout_restart_interval (timeout_t *ut, timeout_time_t interval_ms);

/**\~russian
 * запускает одноразовый таймер с текущего момента.
 * xsec_time - время задержки события таймера
 * вызов   user_timer_arm(, 0) - останавливает таймер, аналогичен user_timer_stop
 * */
void timeout_arm  (timeout_t *ut, timeout_time_t interval_ms);

//* тоже user_timer_arm_us, но не защищен от конкурентности,
//  можно вызывать из прерываиий, если заведомо нет конкуренции
INLINE
void timeout_arm_nomt (timeout_t *ut, timeout_time_t interval){
    ut->cur_time = interval;
    ut->autoreload = 0;
}

/**\~russian
 * запускает одноразовый таймер с момента предыдущего события.
 * xsec_time - время задержки события таймера
 * вызов   user_timer_arm(, 0) - останавливает таймер, аналогичен user_timer_stop
 * !!! если вновь выставленное событие уже просрочено - возвращается true
 * return false - если запущеное событие еще ожидается
 * */
bool_t timeout_rearm  (timeout_t *ut, timeout_time_t interval);

INLINE
bool_t timeout_rearm_nomt (timeout_t *ut, timeout_time_t interval){
    ut->cur_time += interval;
    ut->autoreload = 0;
    return ut->cur_time <=0;
}

#endif //USEC_TIMER



/**\~russian
 * true - если таймер остановлен.
 * !!!не действителен для периодического таймера, работает только с одноразовыми.
 * */
INLINE
bool_t timeout_expired (const timeout_t *ut)
{
    return (ut->cur_time <= 0);
}

/**\~russian
 * возвращает интервал времени до следующего события таймера
 * */
INLINE
timeout_time_t timeout_expiration_timeout(const timeout_t *ut)
{
    return ut->cur_time;
}



#ifdef __cplusplus
}
#endif

#endif //_UOS_TIMEOUT_H_
