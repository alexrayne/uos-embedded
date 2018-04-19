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
#ifndef __1_WIRE_OVER_UART_H__
#define __1_WIRE_OVER_UART_H__

#include <1-wire/1-wire-interface.h>
#include <uart/uart-interface.h>

#ifndef OWOU_MAX_BYTES
#define OWOU_MAX_BYTES      2
#endif

typedef struct 
{
    owif_t          owif;
    
    uartif_t        *uart;
    uint8_t         trx_buffer[OWOU_MAX_BYTES * 8];
} one_wire_over_uart_t;

int one_wire_over_uart_init(one_wire_over_uart_t *owou, uartif_t *uart);

#endif // __1_WIRE_OVER_UART_H__
