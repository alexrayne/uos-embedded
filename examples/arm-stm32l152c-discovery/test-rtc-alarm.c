#include <runtime/lib.h>
#include <kernel/uos.h>
#include <kernel/internal.h>
#include <timer/timer.h>
#include <stm32l1/gpio.h>

ARRAY (task, 1000);
timer_t timer;
mutex_t rtc_alarm;
stm32l1_gpio_t check_pin;

bool_t rtc_alarm_handler (void *arg)
{
    RTC->ISR &= ~(RTC_ALRAF | RTC_RSF);
    EXTI->PR = EXTI_RTC_ALARM;
    arch_intr_allow (IRQ_RTC_ALARM);
    return 0;
}

void hello(void *arg)
{
    //uint16_t ssr;
    
    RTC->ISR |= RTC_INIT;
    
    while (! (RTC->ISR & RTC_INITF));
    RTC->PRER = (RTC->PRER & ~(RTC_PREDIV_A(0x7F) | RTC_PREDIV_S(0x7FFF))) |
        RTC_PREDIV_A(0) | RTC_PREDIV_S(0x7FFF);
    
    RTC->CR &= ~RTC_ALRAE;
    RTC->ALRMAR = RTC_ALRM_MSK4 | RTC_ALRM_MSK3 | RTC_ALRM_MSK2 | RTC_ALRM_MSK1;
    RTC->CR |= RTC_ALRAIE;
    
    RTC->ISR &= ~RTC_INIT;
    
    EXTI->PR = EXTI_RTC_ALARM;
    EXTI->RTSR |= EXTI_RTC_ALARM;
    EXTI->IMR |= EXTI_RTC_ALARM;
    
    mutex_lock_irq(&rtc_alarm, IRQ_RTC_ALARM, rtc_alarm_handler, 0);
    EXTI->IMR &= ~EXTI_RTC_WKUP;
    
    RTC->ALRMASSR = RTC_MASKSS(2) | RTC_SS(0);
    RTC->CR |= RTC_ALRAE;
    
    for (;;) {
        //mutex_wait(&timer.lock);
        //gpio_set_val(to_gpioif(&check_pin), 1);

        //RTC->CR &= ~RTC_ALRAE;
        //debug_printf("RTC_CR1 = %08X\n", RTC->CR);
        //ssr = RTC->SSR;
        //debug_printf("ssr1 = %d\n", ssr);
        //if (ssr != 0) {
        //    RTC->ALRMASSR = RTC_MASKSS(2) | RTC_SS(0);
            //debug_printf("ssr2 = %d\n", RTC->SSR);
        //}
        //else
        //    RTC->ALRMASSR = RTC_MASKSS(15) | RTC_SS(0x7FFF);
        //RTC->CR |= RTC_ALRAE;
        //debug_printf("RTC_CR2 = %08X, ALRMASRR = %08X\n", RTC->CR, RTC->ALRMASSR);
        
        //debug_printf("ssr3 = %d\n", RTC->SSR);
        mutex_wait(&rtc_alarm);
        gpio_set_val(to_gpioif(&check_pin), 1);
        gpio_set_val(to_gpioif(&check_pin), 0);
    }
}

void uos_init(void)
{
    debug_printf("\nTesting real time clock alarm.\n");
    timer_init_ns (&timer, KHZ, 3906250);
    stm32l1_gpio_init(&check_pin, GPIO_PORT_C, 3, GPIO_FLAGS_OUTPUT);
    task_create(hello, "Timer", "hello", 1, task, sizeof (task));
}

