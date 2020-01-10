#ifndef __TIME_H__
#define __TIME_H__

#include <rtc/rtc-interface.h>

typedef uint32_t    time_t;
typedef struct _ntp_time_t ntp_time_t;

struct _ntp_time_t
{
    uint32_t seconds;
    uint32_t fraction;
};

time_t datetime_to_time_t(const datetime_t *dt);
void time_t_to_datetime(time_t time, datetime_t *dt);

ntp_time_t datetime_to_ntp(const datetime_t *dt);
void ntp_to_datetime(ntp_time_t ntp_time, datetime_t *dt);


#endif //__TIME_H__
