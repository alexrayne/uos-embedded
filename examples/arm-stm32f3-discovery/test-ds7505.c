//
// Тестирование датчика температуры с термостатом DS7505.
//
// Вывод O.S. DS7505 подключается к выводу PB5 микроконтроллера, который
// настраивается как вход с обработкой прерываний. Термостат должен давать
// прерывание при превышении Tos и при снижении температуры ниже Thyst
// (задаются в параметрах функции ds7505_set_thermostat).
#include <runtime/lib.h>
#include <kernel/uos.h>
#include <stm32f3/i2c.h>
#include <stm32f3/gpio.h>
#include <thermo/ds7505.h>
#include <timer/timer.h>

#define SLA     0x90
#define FREQ    400000

ARRAY (task_space, 0x400);
ARRAY (ds7505_intr_space, 0x400);

stm32f3_i2c_t i2c;
stm32f3_gpio_t sda;
stm32f3_gpio_t scl;
stm32f3_gpio_t os;
ds7505_t ds7505;
timer_t timer;


void task (void *arg)
{
    int res;
    
    ds7505_init(&ds7505, to_i2cif(&i2c), SLA, I2C_MODE_FM, FREQ/1000);        

    res = ds7505_set_mode(&ds7505, DS7505_MODE_INTERRUPT | DS7505_OS_ACTIVE_LOW |
        DS7505_OS_DEBOUNCE_4 | DS7505_RESOLUTION_12BITS);
    debug_printf("ds7505_set_mode result: %d\n", res);
    
    res = ds7505_set_thermostat(&ds7505, 28000, 27000);
    debug_printf("ds7505_set_thermostat result: %d\n", res);
    /*
    res = ds7505_burn_settings(&ds7505);
    debug_printf("ds7505_burn_settings result: %d\n", res);
    */
    
    res = ds7505_read_t_os(&ds7505);
    debug_printf("Tos = %d\n", res);
    res = ds7505_read_t_hyst(&ds7505);
    debug_printf("Thyst = %d\n", res);

    for (;;) {
        res = thermo_read(to_thermoif(&ds7505));
        if (res > 0x7FFFFFF0)
            debug_printf("ERROR: %d!\n", res - 0x7FFFFFFF);
        else
            debug_printf("Temperature: %d\n", res);
        timer_delay(&timer, 500);
    }
}

void ds7505_interrupt_task (void *arg)
{
    gpio_attach_interrupt(to_gpioif(&os),
        GPIO_EVENT_RISING_EDGE | GPIO_EVENT_FALLING_EDGE, 0, 0);

    for (;;) {
        mutex_wait(gpio_get_mutex(to_gpioif(&os)));
        int temp = thermo_read(to_thermoif(&ds7505));
        debug_printf("ds7505 interrupt, temperature %d\n", temp);
    }
}

void uos_init (void)
{
    debug_printf("\n\nTesting DS7505\n");
    
    timer_init(&timer, KHZ, 100);

    stm32f3_gpio_init(&scl, GPIO_PORT_B, 6, GPIO_FLAGS_ALT_FUNC(4));
    stm32f3_gpio_init(&sda, GPIO_PORT_B, 7, GPIO_FLAGS_ALT_FUNC(4));
    stm32f3_gpio_init(&os,  GPIO_PORT_B, 5, GPIO_FLAGS_INPUT | GPIO_FLAGS_PULL_UP);
    stm32f3_i2c_init(&i2c, 1);
    
	task_create( task, "task", "task", 2, task_space, sizeof(task_space) );
	task_create( ds7505_interrupt_task, "intr", "intr", 1, ds7505_intr_space, sizeof(ds7505_intr_space) );
}
