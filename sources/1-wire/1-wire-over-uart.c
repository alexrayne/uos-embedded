/*
 * 1-wire over UART driver.
 *
 * Copyright (C) 2018 Dmitry Podkhvatilin <vatilin@gmail.com>
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You can redistribute this file and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software Foundation;
 * either version 2 of the License, or (at your discretion) any later version.
 * See the accompanying file "COPYING.txt" for more details.
 *
 * As a special exception to the GPL, permission is granted for additional
 * uses of the text contained in this file.  See the accompanying file
 * "COPY-UOS.txt" for details.
 */
 
#include <runtime/lib.h>
#include <kernel/uos.h>
#include <1-wire/1-wire-over-uart.h>

static int owou_reset(owif_t *ow)
{
    one_wire_over_uart_t *owou = (one_wire_over_uart_t *) ow;
    int res;
    uint8_t reset_pulse = 0xF0;
    
    mutex_lock(&ow->lock);
    
    res = uart_set_param(owou->uart, UART_BITS(8) | UART_STOP_1, 9600);
    if (res != UART_ERR_OK) {
        mutex_unlock(&ow->lock);
        return OW_ERR_IO;
    }
        
    res = uart_tx(owou->uart, &reset_pulse, 1);
    if (res != 1) {
        mutex_unlock(&ow->lock);
        return OW_ERR_IO;
    }
    
    res = uart_flush_rx(owou->uart);
    if (res != UART_ERR_OK) {
        mutex_unlock(&ow->lock);
        return OW_ERR_IO;
    }

    int timeout = 1000;
    do {
        res = uart_rx(owou->uart, &reset_pulse, 1, 1);
        if (res < 0) {
            mutex_unlock(&ow->lock);
            return OW_ERR_IO;
        }
    } while ((res == 0) && --timeout);
    if (timeout == 0) {
        mutex_unlock(&ow->lock);
        return OW_ERR_IO;
    }
        
    if (reset_pulse == 0xF0) {
        mutex_unlock(&ow->lock);
        return OW_ERR_NO_PRESENCE;
    }
        
    mutex_unlock(&ow->lock);
    
    return OW_ERR_OK;
}

static int owou_trx(owif_t *ow, const void *tx_data, int tx_size, void *rx_data, int rx_size)
{
    if ((tx_size > OWOU_MAX_BYTES) || (rx_size > OWOU_MAX_BYTES))
        return OW_ERR_TOO_LONG;
        
    one_wire_over_uart_t *owou = (one_wire_over_uart_t *) ow;
    int res;
    
    mutex_lock(&ow->lock);
        
    res = uart_set_param(owou->uart, UART_BITS(8) | UART_STOP_1, 115200);
    if (res != UART_ERR_OK) {
        mutex_unlock(&ow->lock);
        return OW_ERR_IO;
    }
    
    if (tx_size) {
        // Инициализация буфера для передачи
        int byte_n, bit_n;
        const uint8_t *tx8 = tx_data;
        uint8_t *trxp = owou->trx_buffer;
        for (byte_n = 0; byte_n < tx_size; ++byte_n)
            for (bit_n = 0; bit_n < 8; ++bit_n)
                *trxp++ = ((tx8[byte_n] & (1 << bit_n)) ? 0xFF : 0x00);
                
        res = uart_tx(owou->uart, owou->trx_buffer, tx_size << 3);
        if (res != tx_size << 3) {
            mutex_unlock(&ow->lock);
            return OW_ERR_IO;
        }
    }
    
    if (rx_size) {
        memset(owou->trx_buffer, 0xFF, rx_size << 3);

        res = uart_flush_rx(owou->uart);
        if (res != UART_ERR_OK) {
            mutex_unlock(&ow->lock);
            return OW_ERR_IO;
        }

        res = uart_tx(owou->uart, owou->trx_buffer, rx_size << 3);
        if (res != rx_size << 3) {
            mutex_unlock(&ow->lock);
            return OW_ERR_IO;
        }

        int cur_size = 0;
        int timeout = 10;
        while ((cur_size < rx_size << 3) && --timeout) {
            res = uart_rx(owou->uart, owou->trx_buffer + cur_size, 
                sizeof(owou->trx_buffer) - cur_size, 1);
            if (res < 0) {
                mutex_unlock(&ow->lock);
                return OW_ERR_IO;
            } else {
                cur_size += res;
            }
            mdelay(1);
        }
        if (timeout == 0) {
            mutex_unlock(&ow->lock);
            return OW_ERR_IO;
        }

        // Расшифровка принятого буфера
        memset(rx_data, 0, rx_size);
        int byte_n, bit_n;
        uint8_t *rx8 = rx_data;
        uint8_t *trxp = owou->trx_buffer;
        for (byte_n = 0; byte_n < rx_size; ++byte_n)
            for (bit_n = 0; bit_n < 8; ++bit_n)
                rx8[byte_n] |= ((*trxp++ == 0xFF) ? (1 << bit_n) : 0);
    }
    
    mutex_unlock(&ow->lock);
    return OW_ERR_OK;
}

int one_wire_over_uart_init(one_wire_over_uart_t *owou, uartif_t *uart)
{
    owou->uart = uart;
    owou->owif.reset = owou_reset;
    owou->owif.trx = owou_trx;
    
    return OW_ERR_OK;
}
