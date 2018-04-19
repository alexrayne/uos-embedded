#ifndef __STM32L1_RTC__
#define __STM32L1_RTC__

#include <rtc/rtc-interface.h>

typedef struct _stm32l1_rtc_t stm32l1_rtc_t;
struct _stm32l1_rtc_t
{
    rtcif_t             rtcif;
};

void stm32l1_rtc_init(stm32l1_rtc_t *stm_rtc);

#endif // __STM32L1_RTC__
