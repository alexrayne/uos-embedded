/*
 * Ethernet controller driver for Milandr 1986VE1T.
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
#include <runtime/lib.h>
#include <kernel/uos.h>
#include <kernel/internal.h>
#include <stream/stream.h>
#include <mem/mem.h>
#include <buf/buf.h>
#include <milandr/eth.h>

#define ETH_FIFO
#define ETH_INTERRUPT

#define ETH_ERROR_OK			 0
#define ETH_ERROR_CLOCK_INIT	-1
#define ETH_ERROR_PHY_RESET		-2

//*** Функция для очистки буферов приемника и передатчика MAC модуля ***
//Буфер приемника 4096 байт
//Буфер передатчика 4096 байт
static void ClearEthFIFO() {
	uint32_t Temp;
	uint32_t *ptr;
	ptr = (uint32_t*) ARM_ETH_BUF_BASE;
	for (Temp = 0; Temp < 2048; Temp++)
		*ptr++ = 0;
}

/*
 * Set default values to Ethernet controller registers.
 */
static int chip_init(eth_t *u) {
	int i, j;
	volatile int counter;
	int flag = 0;
	uint32_t value;
	/* Leds */

	/* Setup Cloсk */
	ARM_RSTCLK->HS_CONTROL |= ARM_HS_CONTROL_HSE2_ON;

	for (j=0;j<KHZ*20/6;j++) {
		counter++; // ~20ms
	}

	i = 0;
	value = ARM_RSTCLK->CLOCK_STATUS;
	while ((value & 0xC) != 0xC) {
		debug_printf("%03d. ARM_RSTCLK->CLOCK_STATUS = %x\n", i, value);
		counter = 0;
		for (j=0;j<KHZ*100/6;j++) {
			counter++; // ~100ms
		}

		if (100 < i++) {
			return ETH_ERROR_CLOCK_INIT;
		}
		flag = 1;
		value = ARM_RSTCLK->CLOCK_STATUS;
	}

	if (flag) {
		debug_printf("%03d. ARM_RSTCLK->CLOCK_STATUS = %x\n", i, value);
		flag = 0;
	}


	ARM_RSTCLK->ETH_CLOCK = ARM_ETH_CLOCK_ETH_EN | ARM_ETH_CLOCK_PHY_EN	| ARM_ETH_CLOCK_PHY_SEL(ARM_ETH_CLOCK_PHY_SEL_HSE2);

	/* Светодиоды */
	/* Green led PB15
	 * Yellow led PB14
	 */
	/*
	 ARM_RSTCLK->PER_CLOCK |= ARM_PER_CLOCK_GPIOD | ARM_PER_CLOCK_GPIOB;
	 ARM_GPIOB->FUNC = (ARM_GPIOB->FUNC
			& ~( ARM_FUNC_MASK(14) | ARM_FUNC_MASK(15)))
			| (ARM_FUNC_PORT(14) | ARM_FUNC_PORT(15));
	ARM_GPIOB->ANALOG |= ARM_DIGITAL(14) | ARM_DIGITAL(15);
	ARM_GPIOB->DATA = 0;
	ARM_GPIOB->OE |= ARM_GPIO_OUT(14) | ARM_GPIO_OUT(15);
	ARM_GPIOB->PWR = ( ARM_GPIOB->PWR & ~( ARM_PWR_MASK(14) | ARM_PWR_MASK(15)))
			| (ARM_PWR_FASTEST(14) | ARM_PWR_FASTEST(15));
        */
	/* Phy init */
	ARM_ETH->PHY_CTRL = 0;
#ifdef SET_ARM_ETH_PHY_MODE
	ARM_ETH->PHY_CTRL =	ARM_ETH_PHY_ADDR(0x0) | ARM_ETH_PHY_MODE(SET_ARM_ETH_PHY_MODE) | ARM_ETH_PHY_NRST;
#else
	ARM_ETH->PHY_CTRL =	ARM_ETH_PHY_ADDR(0x0) | ARM_ETH_PHY_MODE(ARM_ETH_PHY_FULL_AUTO) | ARM_ETH_PHY_NRST;
#endif

	for (j=0;j<KHZ*20/6;j++) {
		counter++; // ~20ms по даташиту выход на рабочий режим через 16ms
	}
	i = 0;
	value = ARM_ETH->PHY_STAT;
	while ((value & 0x10) == 0) {
		debug_printf("%03d. ARM_ETH->PHY_STAT = %x\n", i, value);
		;	//ждем пока модуль в состоянии сброса
		counter = 0;
		for (j=0;j<KHZ*1000/6;j++) {
			counter++; // ~1000ms
		}

		if (1000 < i++) {
			return ETH_ERROR_PHY_RESET;
		}
		flag = 1;
		value = ARM_ETH->PHY_STAT;
	}
	if (flag) {
		debug_printf("%03d. ARM_ETH->PHY_STAT = %x\n", i, value);
	}

	//* MAC */
	ARM_ETH->MAC_ADDR[0] = u->netif.ethaddr[0];
	ARM_ETH->MAC_ADDR[1] = u->netif.ethaddr[1];
	ARM_ETH->MAC_ADDR[2] = u->netif.ethaddr[2];
	ARM_ETH->MAC_ADDR[3] = u->netif.ethaddr[3];
	ARM_ETH->MAC_ADDR[4] = u->netif.ethaddr[4];
	ARM_ETH->MAC_ADDR[5] = u->netif.ethaddr[5];

	/*  MACReset */
	ARM_ETH->HASH[0] = 0;
	ARM_ETH->HASH[1] = 0;
	ARM_ETH->HASH[2] = 0;
	ARM_ETH->HASH[3] = 0x8000;

	ARM_ETH->IPG = 0x0060;
	ARM_ETH->PSC = 0x0050;
	ARM_ETH->BAG = 0x0200;
	ARM_ETH->JITTER_WND = 0x0005;

	ARM_ETH->R_CFG = ARM_ETH_BCA_EN | ARM_ETH_UCA_EN | ARM_ETH_CF_EN
	/*| ARM_ETH_SF_EN*/| ARM_ETH_EVNT_MODE(ARM_ETH_EVNT_RX_DONE);	//0x8406;

	ARM_ETH->X_CFG = ARM_ETH_EVNT_MODE(ARM_ETH_EVNT_TX_DONE) | /*
	 ARM_ETH_PAD_EN |*/ARM_ETH_PRE_EN | ARM_ETH_CRC_EN |
	ARM_ETH_IPG_EN | ARM_ETH_RTRYCNT(15);	//0x81FA;

	ARM_ETH->G_CFG_LOW = ARM_ETH_COLWND(128) | ARM_ETH_BUFF_MODE(
#ifdef ETH_FIFO
			ARM_ETH_BUFF_FIFO
#else
			ARM_ETH_BUFF_LINEAL
#endif
			) | ARM_ETH_DTRM_EN/* | ARM_ETH_PAUSE_EN*/;
	ARM_ETH->G_CFG_HI = ARM_ETH_DBG_XF_EN | ARM_ETH_DBG_RF_EN;

	ARM_ETH->IMR = 0;
	ARM_ETH->IFR = 0xDFFF;

	ARM_ETH->DELIMITER = 0x1000;//4096 байт буфер передатчика, 4096 байт буфер приемника
	ARM_ETH->R_HEAD = 0x0000;
	ARM_ETH->X_TAIL = 0x1000;
	ARM_ETH->R_TAIL = 0x0000;
	ARM_ETH->X_HEAD = 0x1000;

	ARM_ETH->G_CFG_HI |= (ARM_ETH_RRST | ARM_ETH_XRST);
	ClearEthFIFO();
	ARM_ETH->G_CFG_HI &= ~(ARM_ETH_RRST | ARM_ETH_XRST);
#ifdef ETH_INTERRUPT
	ARM_ETH->IMR = ARM_ETH_LC | ARM_ETH_XF_ERR | ARM_ETH_XF_OK | ARM_ETH_XF_UNDF
			| ARM_ETH_MISSED_F | ARM_ETH_RF_OK;	//разрешение прерываний при успешном приеме пакета
#endif // ETH_INTERRUPT

	ARM_ETH->R_CFG |= ARM_ETH_EN;
	ARM_ETH->X_CFG |= ARM_ETH_EN;

	return ETH_ERROR_OK;
}

void eth_debug(eth_t *u, struct _stream_t *stream) {
	unsigned short phy_ctrl, phy_stat, gcfgl, gcfgh;
	unsigned short xcfg, rcfg, stat;
	unsigned short rxbf_head, rxbf_tail;
	unsigned short txbf_head, txbf_tail;
	//unsigned short tx_ctrl, rx_ctrl, rx_len, rx_ptr;
	//unsigned short stat_rx_all, stat_rx_ok, stat_rx_ovf;
	//unsigned short stat_rx_lost, stat_tx_all, stat_tx_ok;

	mutex_lock(&u->netif.lock);
	gcfgl = ARM_ETH->G_CFG_LOW;
	gcfgh = ARM_ETH->G_CFG_HI;
	xcfg = ARM_ETH->X_CFG;
	rcfg = ARM_ETH->R_CFG;
	stat = ARM_ETH->STAT;

	phy_ctrl = ARM_ETH->PHY_CTRL;
	phy_stat = ARM_ETH->PHY_STAT;

	rxbf_head = ARM_ETH->R_HEAD;
	rxbf_tail = ARM_ETH->R_TAIL;
	txbf_head = ARM_ETH->X_HEAD;
	txbf_tail = ARM_ETH->X_TAIL;
	mutex_unlock(&u->netif.lock);

	printf(stream, "GCTRL = %0x8X\n", gcfgl | (gcfgh << 15));
	printf(stream, "X_CFG = %0x4X\n", xcfg);
	printf(stream, "R_CFG = %0x4X\n", rcfg);
	printf(stream, "STAT = %0x4X\n", stat);
	printf(stream, "PHY_CTRL = %0x4X\n", phy_ctrl);
	printf(stream, "PHY_STAT = %0x4X\n", phy_stat);
	printf(stream, "INT_SRC = %0x4X\n", u->intr_flags);
	printf(stream, "RXBF HEAD:TAIL = %04x:%04x\n", rxbf_head, rxbf_tail);
	printf(stream, "TXBF HEAD:TAIL = %04x:%04x\n", txbf_head, txbf_tail);

}

/* Проверка наличия подключенного кабеля */
int eth_get_carrier(eth_t *u) {
	unsigned phy_stat;

	mutex_lock(&u->netif.lock);
	phy_stat = ARM_ETH->PHY_STAT;
	mutex_unlock(&u->netif.lock);

	return (phy_stat & ARM_ETH_PHY_LED_LINK) != 0;
}

long eth_get_speed(eth_t *u, int *duplex) {
	unsigned phy_ctrl, phy_stat;

	mutex_lock(&u->netif.lock);
	phy_ctrl = ARM_ETH->PHY_CTRL;
	phy_stat = ARM_ETH->PHY_STAT;
	mutex_unlock(&u->netif.lock);

	if (!(phy_stat & ARM_ETH_PHY_LED_LINK))
		return 0;
	if (duplex)
		*duplex = !(phy_ctrl & ARM_ETH_PHY_MODE(0));
	return u->netif.bps;
}

/*
 * Set PHY loop-back mode.
 */
void eth_set_loop(eth_t *u, int on) {
	unsigned phy_ctrl;

	mutex_lock(&u->netif.lock);
	phy_ctrl = ARM_ETH->G_CFG_HI;
	if (on) {
		phy_ctrl |= ARM_ETH_DLB;
	} else {
		phy_ctrl &= ~ARM_ETH_DLB;
	}
	ARM_ETH->G_CFG_HI = phy_ctrl;
	mutex_unlock(&u->netif.lock);
}

/* вкл/выкл приёма всех пакетов */
void eth_set_promisc(eth_t *u, int station, int group) {
	mutex_lock(&u->netif.lock);
	unsigned mac_ctrl = ARM_ETH->R_CFG & ~ARM_ETH_AC_EN;
	if (station) {
		/* Accept any unicast. */
		mac_ctrl |= ARM_ETH_AC_EN;
	}
	/* TODO: multicasts. */
	ARM_ETH->R_CFG = mac_ctrl;
	mutex_unlock(&u->netif.lock);
}
/*
 #ifndef ETH_FIFO
 static void chip_copyin(unsigned *src, unsigned *dst, unsigned words){
 unsigned n;
 for (n = 0; n < words; n++)
 dst++ = src++;

 }
 #endif
 */
/*
 * Write the packet to chip memory and start transmission.
 * Deallocate the packet.
 */
static void transmit_packet(eth_t *u, buf_t *p) {

	/* Send the data from the buf chain to the interface,
	 * one buf at a time. The size of the data in each
	 * buf is kept in the ->len variable. */
	/* debug_printf("Transmit, totLen = %d\n", p->tot_len); */

#ifdef ETH_FIFO
	buf_t *q;
	unsigned i, size = 0;
	unsigned char *buf = (unsigned char*) u->txbuf;
	unsigned *src;
	//buf = u->txbuf;
	size = p->tot_len;
	if (size + 4 > ETH_MTU) {
		++u->netif.out_discards;
		++u->out_large;
		debug_printf("large buff: %d bytes\n", p->tot_len);
		return;
	}
	for (q = p; q; q = q->next) {
		assert(q->len > 0);
		memcpy(buf, q->payload, q->len);
		buf += q->len;
	}
	if (size < 60) {
		size = 60;
		/*debug_printf ("txzero %08x, %d bytes\n", u->txbuf + p->tot_len, size - p->tot_len);*/
		memset(u->txbuf + p->tot_len, 0, size - p->tot_len);
	}
	/*debug_printf("Ethernet Tx %d bytes (tot_len = %d)\n", size, p->tot_len);*/
	ARM_ETH_TX_FIFO = size;
	src = (unsigned*) u->txbuf_physaddr;
	unsigned wordSize = (size + 3) / 4;
	for (i = 0; i < wordSize; i++)
		ARM_ETH_TX_FIFO = *src++;
	ARM_ETH_TX_FIFO = 0x0;
	/*debug_printf("Packet transmit\n");*/
	++u->netif.out_packets;
	u->netif.out_bytes += size;

#else
	buf_t *q;
	unsigned i, size = 0;
	unsigned char *buf = (unsigned char*) u->txbuf;
	uint32_t head, tail;
	unsigned *src, *dst;
	//uint16_t space[2];
	uint16_t spaceAfter;// Свбодного мета до конда буфера
	uint16_t spaceBefore;// Свобдного места с начала
	//uint8_t buffer[1518];

	head = ARM_ETH->X_HEAD;
	tail = ARM_ETH->X_TAIL;
	size = p->tot_len;
	//вычисляем кол-во свободного места в буфере передатчика
	if (head > tail) {
		//space[0] = head - tail;
		spaceAfter = head - tail;
		//space[1] = 0;
		spaceBefore = 0;
	} else {
		//space[0] = 0x2000 - tail;
		spaceAfter = 0x2000 - tail;
		//space[1] = head - 0x1000;
		spaceBefore = head - 0x1000;
	}
	//вычислили кол-во свободного места в буфере передатчика

	//debug_printf("Send %d bytes\n", size);
	if (size > (spaceAfter + spaceBefore - 8)) {
		//debug_printf("Smol size transmit buffer\n");
		return;//-8, так как 4 байта занимает поле длины данных и 4 байта занимает поле статуса пакета
	}

	for (q = p; q; q = q->next) {
		/* Copy the packet into the transmit buffer. */
		assert(q->len > 0);
		/*debug_printf ("txcpy %08x <- %08x, %d bytes\n", buf, q->payload, q->len);*/
		memcpy(buf, q->payload, q->len);
		buf += q->len;
	}

	//unsigned len = p->tot_len;
	if (size < 60) {
		size = 60;
		/*debug_printf ("txzero %08x, %d bytes\n", u->txbuf + p->tot_len, len - p->tot_len);*/
		memset(u->txbuf + p->tot_len, 0, size - p->tot_len);
	}

	dst = (unsigned *) (0x38000000 + tail);

	//tmp = size;
	src = (unsigned *) u->txbuf;

	*dst++ = size;
	//space[0] -= 4;
	spaceAfter -= 4;// Свободного места уменьшилось
	if (dst > (unsigned *) (ARM_ETH_BUF_BASE + 0x1FFC))
	dst = (unsigned *) (ARM_ETH_BUF_BASE + ARM_ETH->DELIMITER);

	unsigned words = (size + size % 4) / 4;

	if (size <= spaceAfter) {
		for (i = 0; i < words; i++) {
			*dst++ = *src++;
		}
	} else {
		words -= spaceAfter / 4;
		for (i = 0; i < (spaceAfter / 4); i++) {
			*dst++ = *src++;
		}
		dst = (unsigned*) (ARM_ETH_BUF_BASE + ARM_ETH->DELIMITER);
		for (i = 0; i < words; i++) {
			*dst++ = *src++;
		}
	}
	if (dst > (unsigned *) (ARM_ETH_BUF_BASE + 0x1FFC))
	dst = (unsigned *) (ARM_ETH_BUF_BASE + ARM_ETH->DELIMITER);
	//tmp = 0;
	*dst++ = 0x0;
	if (dst > (unsigned *) (ARM_ETH_BUF_BASE + 0x1FFC))
	dst = (unsigned *) (ARM_ETH_BUF_BASE + ARM_ETH->DELIMITER);

	ARM_ETH->X_TAIL = (uint16_t) dst;

	++u->netif.out_packets;
	u->netif.out_bytes += size;
#endif	// ETH_FIFO

}

/*
 * Do the actual transmission of the packet. The packet is contained
 * in the pbuf that is passed to the function. This pbuf might be chained.
 * Return 1 when the packet is succesfully queued for transmission.
 * Or return 0 if the packet is lost.
 */
static bool_t eth_output(eth_t *u, buf_t *p, small_uint_t prio) {
	mutex_lock(&u->netif.lock);
	/* debug_printf("From tcp out\n"); */
	/* Exit if link has failed */
	if (p->tot_len < 4 || p->tot_len > ETH_MTU /*||
	 ! (phy_read (u, PHY_STS) & PHY_STS_LINK)*/) {
		++u->netif.out_errors;
		mutex_unlock(&u->netif.lock);
		/*debug_printf ("output: transmit %d bytes, link failed\n", p->tot_len);*/
		netif_free_buf (&u->netif, p);
		return 0;
	}
	/*debug_printf ("output: transmit %d bytes\n", p->tot_len);*/

	if (ARM_ETH->STAT & ARM_ETH_X_EMPTY) {
		/* Смело отсылаем. */
		transmit_packet(u, p);
		mutex_unlock(&u->netif.lock);
		netif_free_buf (&u->netif, p);
		return 1;
	}

	/* Занято, ставим в очередь. */
	if (buf_queue_is_full(&u->outq)) {
	 // Нет места в очереди: теряем пакет.
	 ++u->netif.out_discards;
	 ++u->out_full_buf;
	 mutex_unlock(&u->netif.lock);
	 netif_free_buf (&u->netif, p);
	 return 0;
	 }
	//while (buf_queue_is_full(&u->outq)) // можем навечно тут зависнуть Pavel
	//	asm volatile ("nop;");
	buf_queue_put(&u->outq, p);
	mutex_unlock(&u->netif.lock);
	return 1;
}

/*
 * Get a packet from input queue.
 */
static buf_t *
eth_input(eth_t *u) {
	buf_t *p;

	mutex_lock(&u->netif.lock);
	p = buf_queue_get(&u->inq);
	mutex_unlock(&u->netif.lock);
	return p;
}

/*
 * Setup MAC address.
 */
static void eth_set_address(eth_t *u, unsigned char *addr) {
	mutex_lock(&u->netif.lock);
	memcpy(&u->netif.ethaddr, addr, 6);
	ARM_ETH->MAC_ADDR[0] = u->netif.ethaddr[0];
	ARM_ETH->MAC_ADDR[1] = u->netif.ethaddr[1];
	ARM_ETH->MAC_ADDR[2] = u->netif.ethaddr[2];
	ARM_ETH->MAC_ADDR[3] = u->netif.ethaddr[3];
	ARM_ETH->MAC_ADDR[4] = u->netif.ethaddr[4];
	ARM_ETH->MAC_ADDR[5] = u->netif.ethaddr[5];
	mutex_unlock(&u->netif.lock);
}

/*
 #ifdef ETH_FIFO
 static void chip_read_rxfifo(eth_t *u, unsigned nbytes) {
 unsigned * dst = (unsigned *) u->rxbuf_physaddr;

 debug_printf("Rx %d bytes\n", nbytes);
 uint32_t n, size;
 size = (nbytes + nbytes % 4) / 4;
 if (nbytes < ETH_MTU)
 for (n = 0; n < size; n++)
 *dst++ = ARM_ETH_RX_FIFO;
 ARM_ETH->STAT -= 0x20;
 }

 #endif
 */

/*
 * Прочитать принятый пакет из буфера контроллера
 * Положить его во входную очередь.
 */

static void receive_packet(eth_t *u) {

#ifdef ETH_FIFO
	unsigned *dst = (unsigned *) u->rxbuf_physaddr;  // буфер для приёма
	uint32_t Rx_Stat, sizeBytes, sizeWords, i;
	Rx_Stat = ARM_ETH_RX_FIFO; /* Прочитать состояние приёмника */
	sizeBytes = ARM_ETH_PKT_LENGTH(Rx_Stat);
	if (sizeBytes - 4 > ETH_MTU) {
		++u->netif.in_errors;
		++u->in_discards_len_packet;
		ARM_ETH->STAT = 0;
		return;
	}
	sizeWords = (sizeBytes + 3) / 4;
	for (i = 0; i < sizeWords; i++) {
		*dst++ = ARM_ETH_RX_FIFO;
	}
	ARM_ETH->STAT = 0;
	if (Rx_Stat	& (ARM_ETH_PKT_SMB_ERR | ARM_ETH_PKT_CRC_ERR | ARM_ETH_PKT_DN_ERR | ARM_ETH_PKT_LF_ERR)) {
		++u->netif.in_errors;
		return;
	}

	++u->netif.in_packets;
	u->netif.in_bytes += sizeBytes;

	if (buf_queue_is_full(&u->inq)) {
		++u->netif.in_discards;
		++u->in_discards_full_buff;
		return;
	}

	buf_t *p = buf_alloc(u->pool, sizeBytes, 4); /* выделить фактически места */
	if (!p) {
		++u->netif.in_discards;
		++u->in_discards_full_buff;
		return;
	}
	memcpy(p->payload, u->rxbuf, sizeBytes);
	buf_queue_put(&u->inq, p);

#else

	uint16_t space_end = 0, tail, head;
	unsigned *src, *dst;
	uint32_t size, i, buf;
	uint16_t tmp[2], len;
	tail = ARM_ETH->R_TAIL;
	head = ARM_ETH->R_HEAD;

	if (tail > head) {
		space_end = tail - head;
	} else {
		space_end = ARM_ETH->DELIMITER - head;
	}

	src = (uint32_t*) (ARM_ETH_BUF_BASE + head);

	*((uint32_t*) tmp) = *src++;

	len = tmp[0];
	//debug_printf("Recive packet \tRx_stat = 0x%08x; \tbytes = %d\n",
	//		*((uint32_t*) tmp), tmp[0]);

	if ((len - 4) > ETH_MTU) {
		/*debug_printf("receive_data: len > MTU <%d>\n", len);*/
		++u->netif.in_discards;
		++u->in_discards_len_packet;
		ARM_ETH->R_HEAD = ARM_ETH->R_TAIL;
		ARM_ETH->STAT -= 0x20;
		return;
	}
	if ((tmp[1] << 16)
			& (ARM_ETH_PKT_SMB_ERR | ARM_ETH_PKT_CRC_ERR | ARM_ETH_PKT_DN_ERR
					/*| ARM_ETH_PKT_LEN_ERR | ARM_ETH_PKT_SF_ERR */
					| ARM_ETH_PKT_LF_ERR)) {
		debug_printf("Error  <0x%08X>\tsize = %d\n", tmp[1] << 16, tmp[0]);
		++u->netif.in_errors;
		ARM_ETH->R_HEAD = ARM_ETH->R_TAIL;
		ARM_ETH->STAT = 0;
		return;
	}
	u->netif.in_bytes += len;
	++u->netif.in_packets;
	space_end -= 4;

	if ((uint16_t) src > (ARM_ETH->DELIMITER - 1))
	src = (uint32_t*) ARM_ETH_BUF_BASE;

	if (buf_queue_is_full(&u->inq)) {
		/*debug_printf("receive_data: input overflow\n");*/
		++u->netif.in_discards;
		++u->in_discards_full_buff;
		ARM_ETH->R_HEAD = ARM_ETH->R_TAIL;
		ARM_ETH->STAT -= 0x20;
		return;
	}

	buf_t *p = buf_alloc(u->pool, len, 4); /* выделить фактически места */
	if (!p) {
		/* debug_printf("receive_data: ignore packet - out of memory\n"); */
		++u->netif.in_discards;
		++u->in_discards_full_buff;
		ARM_ETH->R_HEAD = ARM_ETH->R_TAIL;
		ARM_ETH->STAT -= 0x20;
		return;
	}
	dst = (unsigned *) &p->payload[0];

	//debug_printf("Rx frame, len = %d\tdst=0x%08X\n", len + 4, dst);
	size = (tmp[0] + 3) / 4;

	if (tmp[0] <= space_end) {
		for (i = 0; i < (size - 1); i++)
		*dst++ = *src++;

		buf = *src++;
	} else {
		size = size - space_end / 4;

		for (i = 0; i < (space_end / 4); i++)
		*dst++ = *src++;

		src = (uint32_t*) ARM_ETH_BUF_BASE;

		for (i = 0; i < (size - 1); i++)
		*dst++ = *src++;

		buf = *src++;
	}

	if ((uint16_t) src > (ARM_ETH->DELIMITER - 1))
	src = (uint32_t*) ARM_ETH_BUF_BASE;
	ARM_ETH->R_HEAD = (uint16_t) src;
	/* printf(&debug, "R_HEAD = <%04X> \tR_TAIL = <%04X>\n", ARM_ETH->R_HEAD, ARM_ETH->R_TAIL); */
	ARM_ETH->STAT = 0;
	buf_queue_put(&u->inq, p);
#endif //ETH_FIFO

}

/*
 * Process an interrupt.
 * Return nonzero when there was some activity.
 */
static unsigned handle_interrupt(eth_t *u) {
	unsigned active = 0;

	ARM_NVIC_ICPR(0) = 1 << ETH_IRQ;
	u->intr_flags = ARM_ETH->IFR;
	if (u->intr_flags & ARM_ETH_XF_UNDF) {
		ARM_ETH->R_CFG &= ~ARM_ETH_EN;
		ARM_ETH->X_CFG &= ~ARM_ETH_EN;
		ARM_ETH->G_CFG_HI |= ARM_ETH_XRST;
		ARM_ETH->X_TAIL = ARM_ETH->X_HEAD;
		ARM_ETH->G_CFG_HI &= ~ARM_ETH_XRST;
		ARM_ETH->X_CFG |= ARM_ETH_EN;
		ARM_ETH->R_CFG |= ARM_ETH_EN;
	}
	/* Обработка приёма пакета */
	//if (ARM_ETH->X_TAIL != ARM_ETH->X_HEAD) {
	//	/* Если есть данные, прочитать пакет */
	//	receive_packet(u);
	//	active++;
	//}
	if (ARM_ETH->R_TAIL != ARM_ETH->R_HEAD) {
		/* Если есть данные, прочитать пакет */
		while (ARM_ETH->STAT & 0xE0) {
			receive_packet(u);
			ARM_ETH->IFR |= ARM_ETH_RF_OK;
		}
		active++;
	}
	u->intr_flags = ARM_ETH->IFR;
	if (u->intr_flags & ARM_ETH_MISSED_F) {
		ARM_ETH->IFR |= ARM_ETH_MISSED_F;
		//ARM_ETH->R_HEAD = ARM_ETH->R_TAIL;
		//if (ARM_ETH->STAT & 0xE0){
		//	ARM_ETH->STAT -= 0x20;
		//	++u->in_missed_and_rx;
		//}
		//if(ARM_ETH->STAT & ~0xE0)
		//	ARM_ETH->R_HEAD = ARM_ETH->R_TAIL;
		//chip_read_rxfifo(u, ARM_ETH_PKT_LENGTH(ARM_ETH_RX_FIFO));
		++u->netif.in_discards;
		++u->in_missed;
		//ARM_NVIC_ICPR(0) = 1 << ETH_IRQ;
		//return 0;
	}
	/* успешная отправка */
	if (u->intr_flags & ARM_ETH_XF_OK) {
		ARM_ETH->IFR |= ARM_ETH_XF_OK;
		/* debug_printf("Transmot ok. ARM_ETH->IFR = <0x%04x>\n", u->intr_flags); */
//#ifdef ETH_FIFO
//		unsigned tmp = ARM_ETH_TX_FIFO;
//#endif
		/* Извлекаем следующий пакет из очереди. */
		buf_t *p = buf_queue_get(&u->outq);
		if (p) {
			/* Передаём следующий пакет. */
			transmit_packet(u, p);
			netif_free_buf (&u->netif, p);
		}
		active++;
	}
	if (u->intr_flags & ARM_ETH_XF_ERR) {
		ARM_ETH->IFR |= ARM_ETH_XF_ERR;
		++u->netif.out_errors;
//#ifdef ETH_FIFO
//		unsigned tmp = ARM_ETH_TX_FIFO;
//#endif
		/* debug_printf("Error Tx on transmiteARM_ETH->IFR = <0x%04x>\n", u->intr_flags); */
	}
	/* Подсчитываем коллизии. */
	if (u->intr_flags & ARM_ETH_LC) {
		ARM_ETH->IFR |= ARM_ETH_LC;
		++u->netif.out_collisions;
		/* debug_printf("Error collisions\n"); */
	}
	//ARM_NVIC_ICPR(0) = 1 << ETH_IRQ;
	return active;
}

/*
 * Poll for interrupts.
 * Must be called by user in case there is a chance to lose an interrupt.
 */
void eth_poll(eth_t *u) {
	uint32_t lock = mutex_trylock(&u->netif.lock);
	if (handle_interrupt(u))
		mutex_signal(&u->netif.lock, 0);
	if (lock)
		mutex_unlock(&u->netif.lock);
}

/*
 * Interrupt task.
 */
static void interrupt_task(void *arg) {
	eth_t *u = arg;

	/* Register the interrupt. */
	mutex_lock_irq(&u->netif.lock, ETH_IRQ, 0, 0);

	for (;;) {
		/* Wait for the interrupt. */
		mutex_wait(&u->netif.lock);
		++u->intr;
		handle_interrupt(u);
	}
}

static netif_interface_t eth_interface = { (bool_t (*)(netif_t*, buf_t*,
		small_uint_t)) eth_output, (buf_t *(*)(netif_t*)) eth_input, (void (*)(
		netif_t*, unsigned char*)) eth_set_address, };

/*
 * Set up the network interface.
 */
void eth_init(eth_t *u, const char *name, int prio, mem_pool_t *pool,
		struct _arp_t *arp, const unsigned char *macaddr) {
	u->netif.interface = &eth_interface;
	u->netif.name = name;
	u->netif.arp = arp;
	u->netif.mtu = ETH_MTU;
	u->netif.type = NETIF_ETHERNET_CSMACD;
	u->netif.bps = 100000;
	memcpy(&u->netif.ethaddr, macaddr, 6);

	u->pool = pool;
	u->rxbuf = (unsigned char*) ((unsigned) (u->rxbuf_data + 7) & ~7);
	u->txbuf = (unsigned char*) ((unsigned) (u->txbuf_data + 7) & ~7);
	u->rxbuf_physaddr = (unsigned) u->rxbuf;
	u->txbuf_physaddr = (unsigned) u->txbuf;

	buf_queue_init(&u->inq, u->inqdata, sizeof(u->inqdata));
	buf_queue_init(&u->outq, u->outqdata, sizeof(u->outqdata));
	/*u->rxbuf_data[0] = 1;
	 u->rxbuf_data[1] = 2;
	 u->rxbuf_data[2] = 3;
	 u->rxbuf_data[3] = 4;
	 u->rxbuf_data[4] = 5;
	 u->rxbuf_data[5] = 6;
	 u->rxbuf_data[6] = 7;
	 u->rxbuf_data[7] = 8;
	 debug_printf("buf addres rxbuf_data = 0x%08X\nrx_uf = 0x%08X\nphysaddr = 0x%08X\n", &u->rxbuf_data[0], u->rxbuf, u->rxbuf_physaddr);
	 unsigned *dst;
	 dst = u->rxbuf_physaddr;
	 debug_printf("*dst = 0x%08X (0x%08X)\n", *dst, dst);
	 */
	/* Initialize hardware. */
	int ret;
	ret = chip_init(u);
	if (ret != ETH_ERROR_OK) {
		switch(ret) {
		case ETH_ERROR_CLOCK_INIT:
			debug_printf("\nETH_ERROR_CLOCK_INIT\n");
			break;
		case ETH_ERROR_PHY_RESET:
			debug_printf("\nETH_ERROR_PHY_RESET\n");
			break;
		default:
			debug_printf("\nETH_ERROR UNDEFINED\n");
			break;
		}
		uos_halt (0);
	}

	/* Create interrupt task. */
	u->task_eth_handler = task_create(interrupt_task, u, "eth", prio, u->stack,
			sizeof(u->stack));
}

