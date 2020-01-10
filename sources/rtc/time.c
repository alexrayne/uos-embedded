#include <runtime/lib.h>
#include "time.h"

static uint32_t count_seconds(const datetime_t *dt,
    unsigned nb_years, unsigned nb_leap_years)
{
    unsigned nb_days_since_1jan = dt->day - 1;
    switch (dt->month) {
        case 2:  nb_days_since_1jan += 31;  break;
        case 3:  nb_days_since_1jan += 59;  break;
        case 4:  nb_days_since_1jan += 90;  break;
        case 5:  nb_days_since_1jan += 120; break;
        case 6:  nb_days_since_1jan += 151; break;
        case 7:  nb_days_since_1jan += 181; break;
        case 8:  nb_days_since_1jan += 212; break;
        case 9:  nb_days_since_1jan += 243; break;
        case 10: nb_days_since_1jan += 273; break;
        case 11: nb_days_since_1jan += 304; break;
        case 12: nb_days_since_1jan += 334; break;
        default:
            break;
    }
    
    if ((dt->year % 4 == 0) && (dt->month > 2))
        nb_days_since_1jan += 1; // високосный год, февраль прошёл
        
    unsigned nb_days = nb_leap_years * 366 +
        (nb_years - nb_leap_years) * 365 + nb_days_since_1jan;
    unsigned nb_seconds_this_day = dt->hour * 3600 + dt->minute * 60 +
        dt->second;

    return nb_days * 86400 + nb_seconds_this_day;
}

time_t datetime_to_time_t(const datetime_t *dt)
{
    unsigned nb_years_since_1970 = dt->year - 1970;
    unsigned nb_leap_years = (nb_years_since_1970 + 1) / 4;  // 2000 - високосный, 
                                                             // а дальше, на 100 лет вперёд, мы не смотрим
                                                             // (для 2100 формула неправильная).
    return count_seconds(dt, nb_years_since_1970, nb_leap_years);
}

ntp_time_t datetime_to_ntp(const datetime_t *dt)
{
    unsigned nb_years_since_1900 = dt->year - 1900;
    unsigned nb_leap_years = (nb_years_since_1900 - 1) / 4;  // 1900 - не високосный, 2000 - високосный, 
                                                             // а дальше, на 100 лет вперёд, мы не смотрим
                                                             // (для 2100 формула неправильная).
    
    ntp_time_t result;
    result.seconds = count_seconds(dt, nb_years_since_1900, nb_leap_years);
    result.fraction = (1ull << 32) * dt->nsecond / 1000000000;
    
    return result;
}

static void seconds_to_datetime(uint32_t seconds, 
    unsigned nb_days_before_2016, datetime_t *dt)
{
    dt->second = seconds % 60;
    unsigned nb_minutes = seconds / 60;
    dt->minute = nb_minutes % 60;
    unsigned nb_hours = nb_minutes / 60;
    dt->hour = nb_hours % 24;
    unsigned nb_days = nb_hours / 24;
    unsigned days = nb_days - nb_days_before_2016;
    
    // Добавляем годы, начиная с 2016
    unsigned year = 16;
    while (1) {
        if (year % 4 == 0) { // висококосный год
            if (days >= 366) {
                days -= 366;
                year++;
            } else {
                break;
            }
        } else { // невисокосный год
            if (days >= 365) {
                days -= 365;
                year++;
            } else {
                break;
            }
        }
    }
    
    // Считаем количество месяцев в текущем году
    unsigned month;
    unsigned month_days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30};
    if (year % 4 == 0)
        month_days[2] = 29;
    
    for (month = 1; month < 12; ++month) {
        if (days >= month_days[month])
            days -= month_days[month];
        else break;
    }
    // days теперь содержит число дней в текущем месяце, считая от 0.
    days += 1;
    
    dt->year = 2000 + year;
    dt->month = month;
    dt->day = days;
    dt->nsecond = 0;
}

void time_t_to_datetime(time_t time, datetime_t *dt)
{
    unsigned nb_days_1970_2015 = 11 * 366 + 35 * 365;
    seconds_to_datetime(time, nb_days_1970_2015, dt);
}

void ntp_to_datetime(ntp_time_t ntp_time, datetime_t *dt)
{
    unsigned nb_days_1900_2015 = 28 * 366 + 88 * 365;
    seconds_to_datetime(ntp_time.seconds, nb_days_1900_2015, dt);
    dt->nsecond = ((uint64_t)ntp_time.fraction * 1000000000) >> 32;
}
