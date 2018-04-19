#ifndef __RTC_INTERFACE_H__
#define __RTC_INTERFACE_H__

typedef struct _datetime_t datetime_t;
typedef struct _rtcif_t rtcif_t;

struct _datetime_t
{
    unsigned year;
    unsigned month;
    unsigned day;
    unsigned hour;
    unsigned minute;
    unsigned second;
    unsigned nsecond;
};

// Успешно
#define RTC_ERR_OK              0
// Возвращается в случае передачи функции неверного параметра.
#define RTC_ERR_BAD_PARAM       -1
// Возвращается в случае ошибки по линии связи
#define RTC_ERR_IO              -2
// Возвращается, если функция не поддерживается
#define RTC_ERR_NOT_SUPP        -3

struct _rtcif_t
{
    int (* get_time)(rtcif_t *rtc, datetime_t *dt);
    int (* set_time)(rtcif_t *rtc, const datetime_t *dt);
};

#define to_rtcif(x)   ((rtcif_t*)&(x)->rtcif)


static inline __attribute__((always_inline))
int rtc_get_time(rtcif_t *rtc, datetime_t *dt)
{
	return rtc->get_time(rtc, dt);
}

static inline __attribute__((always_inline))
int rtc_set_time(rtcif_t *rtc, const datetime_t *dt)
{
	return rtc->set_time(rtc, dt);
}


#endif //__RTC_INTERFACE_H__
