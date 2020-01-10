#include <runtime/lib.h>
#include <kernel/uos.h>
#include <timer/timer.h>
#include <stm32l/rtc.h>

ARRAY (task, 1000);
timer_t timer;
datetime_t dt;
ntp_time_t ntp_time;
time_t timet;

void hello (void *arg)
{
    /*
    dt.year = 2016;
    dt.month = 5;
    dt.day = 9;
    dt.hour = 23;
    dt.minute = 36;
    dt.second = 36;
    */

    /*    
    ntp_time.seconds = 0xdadb9067;
    ntp_time.fraction = 0x7bcd7420;
    rtc_ntp_to_datetime(ntp_time, &dt);
    */
    
    timet = 0x573112B0;
    rtc_time_t_to_datetime(timet, &dt);
    
    rtc_set_time(&dt);
    
	for (;;) {
		mutex_wait (&timer.lock);
		rtc_get_time(&dt);
		ntp_time = rtc_datetime_to_ntp(&dt);
		debug_printf("%d.%02d.%02d %02d:%02d:%02d - NTP: %08X.%08X, time_t: %08X\n",
		    dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second, 
		    ntp_time.seconds, ntp_time.fraction, rtc_datetime_to_time_t(&dt));
	}
}

void uos_init (void)
{
	debug_printf ("\nTesting real time clock.\n");
	timer_init (&timer, KHZ, 1000);
	rtc_init();
	task_create (hello, "Timer", "hello", 1, task, sizeof (task));
}

