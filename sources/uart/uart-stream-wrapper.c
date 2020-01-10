#include <runtime/lib.h>
#include <kernel/uos.h>
#include <uart/uart-stream-wrapper.h>

static void uart_stream_putchar(uart_stream_wrapper_t *w, short c)
{
    uart_tx(w->uartif, &c, 1);
    if (c == '\n') {
        char ret = '\r';
        uart_tx(w->uartif, &ret, 1);
    }
}

static unsigned short uart_stream_getchar(uart_stream_wrapper_t *w)
{
    unsigned short c;
    if (w->last_char != -1) {
        w->last_char = -1;
        c = w->last_char;
    } else {
        uart_rx(w->uartif, &c, 1, 0);
    }
    return c;
}

static int uart_stream_peekchar(uart_stream_wrapper_t *w)
{
    int c;
    int res;
    res = uart_rx(w->uartif, &c, 1, 1);
    if (res > 0) {
        w->last_char = c;
        return c;
    } else {
        return -1;
    }
}

static void uart_stream_fflush(uart_stream_wrapper_t *w)
{
}

static mutex_t *uart_stream_receive_lock(uart_stream_wrapper_t *w)
{
    return &w->uartif->lock;
}

static stream_interface_t stream_interface = {
    .putc = (void (*) (stream_t*, short))       uart_stream_putchar,
    .getc = (unsigned short (*) (stream_t*))    uart_stream_getchar,
    .peekc = (int (*) (stream_t*))              uart_stream_peekchar,
    .flush = (void (*) (stream_t*))             uart_stream_fflush,
    .receiver = (mutex_t *(*) (stream_t*))      uart_stream_receive_lock,
};


void uart_stream_wrapper_init(uart_stream_wrapper_t *wrapper, uartif_t *uart)
{
    wrapper->interface = &stream_interface;
    wrapper->uartif    = uart;
    wrapper->last_char = -1;
}
