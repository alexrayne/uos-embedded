/*
 * Ethernet controller driver for Milandr 1986BE1T.
 *
 * Copyright (C) 2010 Serge Vakulenko, <serge@vak.ru>
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
#ifndef __1986VE1T_ETH_H
#define __1986VE1T_ETH_H

#include <net/netif.h>
#include <buf/buf-queue.h>

#ifndef ETH_STACKSZ
#   define ETH_STACKSZ	800
#endif

typedef volatile unsigned int eth_reg_t;

#define ETH_IRQ			ETHERNET_IRQn		/* pin PB10 - EXT_INT2 */
#define ETH_MTU			1500		/* maximum ethernet frame length */
#define MDR_ETHERNET1_BUF_SIZE		8192
#define RXBUF_BYTES     4096		/* size of hardware receive buffer */

struct _mem_pool_t;
struct _stream_t *stream;

typedef struct _eth_t {
	netif_t netif;			/* common network interface part */
	struct _mem_pool_t *pool;	/* memory pool for allocating packets */

	buf_queue_t inq;		/* queue of received packets */
	struct _buf_t *inqdata[8];

	buf_queue_t outq;		/* queue of packets to transmit */
	struct _buf_t *outqdata[8];
	task_t *task_eth_handler;

	unsigned intr_flags;		/* interrupt flags */
	unsigned long intr;		/* interrupt counter */
	unsigned char rxbuf_data [ETH_MTU + 8 + 4];
	unsigned char txbuf_data [ETH_MTU + 8 + 4];
	unsigned char *rxbuf;		/* aligned rxbuf[] */
	unsigned char *txbuf;		/* aligned txbuf[] */
	unsigned rxbuf_physaddr;	/* phys address of rxbuf[] */
	unsigned txbuf_physaddr;	/* phys address of txbuf[] */

	unsigned long in_discards_len_packet;
	unsigned long in_discards_full_buff;
	unsigned long in_missed;
	unsigned long in_missed_and_rx;
	unsigned long out_large;
	unsigned long out_full_buf;
	ARRAY (stack, ETH_STACKSZ); /* stack for irq task */
} eth_t;

void eth_init (eth_t *u, const char *name, int prio,
	struct _mem_pool_t *pool, struct _arp_t *arp, const unsigned char *macaddr);
void eth_debug (eth_t *u, struct _stream_t *stream);
int eth_get_carrier (eth_t *u);
long eth_get_speed (eth_t *u, int *duplex);
void eth_set_loop (eth_t *u, int on);
void eth_set_promisc (eth_t *u, int station, int group);
void eth_poll (eth_t *u);

#endif /* __1986VE1T_ETH_H */
