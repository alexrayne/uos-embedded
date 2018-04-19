//
// Тестирование внешней микросхемы DS1307, подключенной по шине i2c.
//
#include <runtime/lib.h>
#include <kernel/uos.h>
#include <elvees/i2c.h>
#include <rtc/ds1307.h>
#include <timer/timer.h>

ARRAY (task_space, 0x400);

elvees_i2c_t i2c;
ds1307_t ds1307;
timer_t timer;

datetime_t dt;

void task (void *arg)
{
    int res;

    dt.year = 2018;
    dt.month = 2;
    dt.day = 8;
    dt.hour = 18;
    dt.minute = 57;
    dt.second = 30;
    debug_printf("Setting date to: %d.%02d.%02d %02d:%02d:%02d\n",
        dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);

    res = rtc_set_time(to_rtcif(&ds1307), &dt);
    if (res == RTC_ERR_OK)
	    debug_printf("OK\n");
    else
        debug_printf("ERROR: %d!\n", res);
    
    for (;;) {
        res = rtc_get_time(to_rtcif(&ds1307), &dt);
        if (res == RTC_ERR_OK)
		    debug_printf("%d.%02d.%02d %02d:%02d:%02d\n",
		        dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
        else
            debug_printf("ERROR: %d!\n", res);
            
        timer_delay(&timer, 1000);
    }
}

void uos_init (void)
{
    debug_printf("\n\nTesting DS1307\n");
    
    timer_init(&timer, KHZ, 100);

    elvees_i2c_init(&i2c);
    ds1307_init(&ds1307, to_i2cif(&i2c));
    
	task_create( task, "task", "task", 2, task_space, sizeof(task_space) );
}
