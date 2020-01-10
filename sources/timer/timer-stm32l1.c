#include <timer/timer-private.h>

static inline void
do_set_period_rtc(arm_reg_t wutr)
{
    /* Clear write protection for RTC registers */
    RCC->APB1ENR |= RCC_PWREN;
    PWR->CR |= PWR_DBP;
    RTC->WPR = 0xCA;
    RTC->WPR = 0x53;
    /* Enable LSE and set it as clock source for RTC */
    RCC->CSR |= RCC_LSEON;
    while (! (RCC->CSR & RCC_LSERDY));
    RCC->CSR |= RCC_RTCEN | RCC_RTCSEL_LSE;
    /* Enable RTC Wakeup interrupt in the EXTI */
    EXTI->PR = EXTI_RTC_WKUP;
    EXTI->RTSR |= EXTI_RTC_WKUP;
    EXTI->IMR |= EXTI_RTC_WKUP;
    /* HZ_CLKIN_RTC is divided by 2 by WUT Prescaler */
    RTC->CR = 0;
    while (! (RTC->ISR & RTC_WUTWF));
    RTC->WUTR = wutr;
    RTC->CR = RTC_WUCKSEL_DIV2 | RTC_WUTE | RTC_WUTIE;
}

void
hw_set_period(unsigned long khz, unsigned long new_period)
{
#if defined(NSEC_TIMER)
    do_set_period_rtc((uint64_t)(HZ_CLKIN_RTC / 2) * new_period / 1000000000 - 1);
#elif defined(USEC_TIMER)
    do_set_period_rtc((HZ_CLKIN_RTC / 2) * new_period / 1000000 - 1);
#else
    do_set_period_rtc((HZ_CLKIN_RTC / 2) * new_period / 1000 - 1);
#endif
}

void
hw_clear_interrupt()
{
    RTC->ISR &= ~(RTC_WUTF | RTC_RSF);
    EXTI->PR = EXTI_RTC_WKUP;
}
