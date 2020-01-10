#ifndef __UART_STREAM_WRAPPER_H__
#define __UART_STREAM_WRAPPER_H__

#include <stream/stream.h>
#include <uart/uart-interface.h>

typedef struct _uart_stream_wrapper_t
{
    stream_interface_t *interface;
    uartif_t           *uartif;
    int                 last_char;
} uart_stream_wrapper_t;

void uart_stream_wrapper_init(uart_stream_wrapper_t *wrapper, uartif_t *uart);

#endif // __UART_STREAM_WRAPPER_H__
