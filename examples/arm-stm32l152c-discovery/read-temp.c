/*
 * Reading the temperature sensor
 */
#include <runtime/lib.h>
#include <kernel/uos.h>
#include <timer/timer.h>
#include <stm32l1/gpio.h>

ARRAY (task_space, 0x400);	/* Memory for task stack */
timer_t timer;

stm32l1_gpio_t di_drive;
stm32l1_gpio_t di;


static inline float do_read_temp(float vdda)
{
    int adc_value, adc_value2;
    
#ifndef CLK_SOURCE_HSI
    // Включаем генератор HSI для работы АЦП
    RCC->CR |= RCC_HSION;
    while(! (RCC->CR & RCC_HSIRDY));
#endif

    // Включаем АЦП
    ADC->CR2 |= ADC_ADON;
    ADC->CCR |= ADC_TSVREFE;
    mdelay(1);
    while(! (ADC->SR & ADC_ADONS));

    // Измерение на 16-м канале АЦП
    ADC->SQR5 = ADC_SQ1(16);
    ADC->SMPR2 = ADC_SMP_384CYCLES(16);
    ADC->CR2 |= ADC_SWSTART;
    while (! (ADC->SR & ADC_EOC));
    adc_value = ADC->DR;
    ADC->CCR &= ~ADC_TSVREFE;
    
    ////////////////////////////////
    
    // Переключаем вывод в режим аналогового входа    
    stm32l1_gpio_init(&di, GPIO_PORT_C, 3, GPIO_FLAGS_ANALOG);
    
    udelay(10);
    
    // Измерение на DI1
    ADC->SQR5 = ADC_SQ1(13);
    ADC->SMPR2 = ADC_SMP_9CYCLES(13);
    ADC->CR2 |= ADC_SWSTART;
    while (! (ADC->SR & ADC_EOC));
    adc_value2 = ADC->DR;
    
    // Переключаем вывод в режим цифрового входа с подтяжкой вниз (низкое потребление)    
    stm32l1_gpio_init(&di, GPIO_PORT_C, 3, GPIO_FLAGS_INPUT | GPIO_FLAGS_PULL_DOWN);
    
    ////////////////////////////////
    
    // Отключаем всё
    ADC->CR2 = 0;
    ADC->SQR5 = 0;
#ifndef CLK_SOURCE_HSI
    RCC->CR &= ~RCC_HSION;
#endif

    float adc_cor_value = vdda * adc_value / 3.;
    unsigned adc_cor_value2 = vdda / 3. * adc_value2;
    unsigned TS_CAL1 = *((unsigned short *) 0x1FF800FA);
    unsigned TS_CAL2 = *((unsigned short *) 0x1FF800FE);
    int temp = (110. - 30.) * (adc_cor_value - TS_CAL1) / (TS_CAL2 - TS_CAL1) + 30.;

    debug_printf ("TS_CAL1: %d, TS_CAL2: %d, ADC code: %d, cor: %d, temp: %d.%02d, di: %d\n",
        TS_CAL1, TS_CAL2, adc_value, (int)adc_cor_value, (int)temp, (int)((temp - (int)temp) * 100), adc_cor_value2);

    return adc_value;
}

static inline float do_read_vdda()
{
    int adc_value;
    
#ifndef CLK_SOURCE_HSI
    // Включаем генератор HSI для работы АЦП
    RCC->CR |= RCC_HSION;
    while(! (RCC->CR & RCC_HSIRDY));
#endif

    // Включаем АЦП
    ADC->CR2 |= ADC_ADON;
    ADC->CCR |= ADC_TSVREFE;
    while(! (ADC->SR & ADC_ADONS));

    // Измерение на 17-м канале АЦП
    ADC->SQR5 = ADC_SQ1(17);
    ADC->SMPR2 = ADC_SMP_96CYCLES(17);
    ADC->CR2 |= ADC_SWSTART;
    while (! (ADC->SR & ADC_EOC));
    adc_value = ADC->DR;
    ADC->CCR &= ~ADC_TSVREFE;
    
    // Отключаем всё
    ADC->CR2 = 0;
    ADC->SQR5 = 0;
#ifndef CLK_SOURCE_HSI
    RCC->CR &= ~RCC_HSION;
#endif

    unsigned short VREFINT_CAL = *((unsigned short *) 0x1FF800F8);
    float vdda = 3. * VREFINT_CAL / adc_value;

    debug_printf ("CAL: %d, ADC code: %d, vrefint: %d.%02d\n", VREFINT_CAL, adc_value, (int)vdda, (int)((vdda - (int)vdda) * 100));

    return vdda;
}


void task (void *arg)
{
	for (;;) {
	    timer_delay (&timer, 1000);
	    
	    debug_printf ("in task\n");
		
        while (PWR->CSR & PWR_VOSF);
        PWR->CR = PWR->CR & ~PWR_ULP;
        while (PWR->CSR & PWR_VOSF);
        
        float vdda = do_read_vdda ();
        do_read_temp (vdda);

        while (PWR->CSR & PWR_VOSF);
        PWR->CR = PWR->CR | PWR_ULP;
        while (PWR->CSR & PWR_VOSF);
	}
}

void uos_init (void)
{
    RCC->APB2ENR |= RCC_ADC1EN;         // включаем тактирование АЦП
    
    stm32l1_gpio_init(&di_drive, GPIO_PORT_C, 2, GPIO_FLAGS_OUTPUT | GPIO_FLAGS_2MHZ);
    gpio_set_val( to_gpioif(&di_drive), 1 );

    stm32l1_gpio_init(&di, GPIO_PORT_C, 3, GPIO_FLAGS_INPUT | GPIO_FLAGS_PULL_DOWN);
    
    timer_init (&timer, KHZ, 100);
	task_create (task, "task", "task", 1, task_space, sizeof (task_space));
}
