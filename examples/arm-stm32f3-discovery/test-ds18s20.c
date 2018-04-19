//
// Тестирование датчика температуры DS18S20 (1-wire).
//
#include <runtime/lib.h>
#include <kernel/uos.h>
#include <timer/timer.h>
#include <stm32f3/gpio.h>
#include <stm32f3/uart.h>
#include <1-wire/1-wire-over-uart.h>
#include <thermo/ds18s20.h>

#define UART_PORT   3

ARRAY (task_space, 0x400);

timer_t timer;
stm32f3_gpio_t uart_rx_pin;
stm32f3_gpio_t uart_tx_pin;
stm32f3_uart_t uart;
one_wire_over_uart_t one_wire;
ds18s20_t ds18s20;


void task (void *arg)
{
    int res;

    ds18s20_init(&ds18s20, to_owif(&one_wire), &timer, 800, 1);

    for (;;) {
        res = thermo_read(to_thermoif(&ds18s20));
        if (res > 0x7FFFFFF0)
            debug_printf("ERROR: %d!\n", res - 0x7FFFFFFF);
        else
            debug_printf("Temperature: %d\n", res);
        timer_delay(&timer, 500);
    }
}

void uos_init (void)
{
    debug_printf("\n\nTesting DS18S20\n");
    
    timer_init(&timer, KHZ, 100);
    
    stm32f3_gpio_init(&uart_tx_pin, GPIO_PORT_B, 10, GPIO_FLAGS_ALT_FUNC(7) | GPIO_FLAGS_OPEN_DRAIN | GPIO_FLAGS_PULL_UP);
    stm32f3_gpio_init(&uart_rx_pin, GPIO_PORT_B, 11, GPIO_FLAGS_ALT_FUNC(7));
    stm32f3_uart_init(&uart, UART_PORT);
    one_wire_over_uart_init(&one_wire, to_uartif(&uart));
    
	task_create(task, "task", "task", 2, task_space, sizeof(task_space));
}
