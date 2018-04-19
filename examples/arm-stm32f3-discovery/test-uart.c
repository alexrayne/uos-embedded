/*
 * Testing uart driver.
 */
#include <runtime/lib.h>
#include <kernel/uos.h>
#include <stm32f3/uart.h>

#define PORT    1

#define TX_MESSAGE  (const unsigned char *)"Hello using UART driver!\n\r"

ARRAY (task_space, 0x400);	/* Memory for task stack */

stm32f3_uart_t uart;

void task (void *arg)
{
    char rxbuf[8];
    
    uart_set_param(to_uartif(&uart), UART_BITS(8) | UART_STOP_1, 115200);
    
	for (;;) {
	    uart_tx(to_uartif(&uart), TX_MESSAGE, strlen(TX_MESSAGE));
	    uart_rx(to_uartif(&uart), rxbuf, sizeof(rxbuf), 0);
	}
}

void uos_init (void)
{
    debug_printf("Testing UART interface\n");
    stm32f3_uart_init(&uart, PORT);
	task_create (task, "task", "task", 1, task_space, sizeof (task_space));
}
