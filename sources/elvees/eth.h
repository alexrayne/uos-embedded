/*
 * Ethernet controller driver for Elvees Multicore NVCom.
 * Copyright (c) 2010 Serge Vakulenko.
 * Based on sources from Ildar F Kaibyshev skif@elvees.com.
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
 * 
 * UTF-8 ru-RU
 */
#ifndef __NVCOM_ETH_H
#define __NVCOM_ETH_H

#include <uos-conf-net.h>
#include <net/netif.h>
#include <buf/buf-queue-header.h>

#ifdef __cplusplus
extern "C" {
#endif



#ifndef ETH_STACKSZ
#   define ETH_STACKSZ		1000
#endif

#ifndef ETH_INQ_SIZE
#   define ETH_INQ_SIZE 	16
#endif

#ifndef ETH_OUTQ_SIZE
#   define ETH_OUTQ_SIZE 	8
#endif

/** \~russian этот резерв старается удерживать свободные слоты в буфере передачи
 * отвергая пакеты с оверлапами. позволяет защитить канал передачи от полной загрузки 
 * */
#ifndef ETH_OUTQ_PRESERVE
#   define ETH_OUTQ_PRESERVE  (ETH_OUTQ_SIZE/4)
#endif

#ifndef ETH_MTU
#   define ETH_MTU		1518	/* maximum ethernet frame length */
#endif

#if ETH_OPTIMISE_SPEED > 0

//*этот параметр задает порог длины приемного/передаваемого буфера, с которой начинаем использовать
//*  ожидание на мутехе, при меньшей длине буфера, жду его передачи поллингом.
//*  позволит избежать лишней нагрузки на переключениях контекста
#ifndef ETH_DMA_WAITPOLL_TH
#define ETH_DMA_WAITPOLL_TH    0x200
#endif

#include <stdint.h>
// структура - блок параметров DMA
/*******************************
 63____________________________0
 {IR132,                       };
 { CSR32,           CP32       }.
 ********************************/
//Параметры для самоинициализации DMA портов
typedef struct __attribute__((packed,aligned(8))) _EMAC_PortCh_Settings
{
    uint32_t none;
    uint32_t ir; //данные для регистра индекса (адрес памяти) (IR);
    struct _EMAC_PortCh_Settings* cp; //адрес следующего блока параметров DMA передачи
    uint32_t csr; //данные для регистра управления и состояния CSR
} EMAC_PortCh_Settings;

#if defined(ELVEES_NVCOM02T) || defined (ELVEES_NVCOM02)

#ifndef ETH_TX_CHUNKS
//!!! TODO не тестирован код с ETH_TX_CHUNKS> 0
//!!!   кажется EMACdma вообще не умеет работать с цепями
#define ETH_TX_CHUNKS   0
#endif

// облегчаем жизнь просессору за счет устранения циклов ожидания, позволяет отдать 
//  до 10мкс на пакетах более 512 байт
//  но чуть ухудшает скорость запуска передатчика за счет вхождения в прерывание
#ifndef ETH_TX_USE_DMA_IRQ
#define ETH_TX_USE_DMA_IRQ      1
#endif

#endif // defined(ELVEES_NVCOM02T) || defined (ELVEES_NVCOM02)
#endif // #if ETH_OPTIMISE_SPEED > 0

//* это отпределение вводит task_yield в циклы ожидания, разгружает процессор, но может
//  сильные лаги внести
#define ETH_USE_YELDING_WAIT

struct _mem_pool_t;
typedef struct __attribute__ ((aligned(8))) _eth_t {
	netif_t netif;			/* common network interface part */
	mutex_t tx_lock;		/* get tx interrupts here */
	struct _mem_pool_t *pool;	/* memory pool for allocating packets */

	buf_queue_header_t inq;		/* queue of received packets */
	struct _buf_t *inqdata[ETH_INQ_SIZE];

	buf_queue_header_t outq;		/* queue of packets to transmit */
	struct _buf_t *outqdata[ETH_OUTQ_SIZE];

    struct {
        unsigned adr;           /* address of external PHY */
        unsigned last_status;
#if ETH_OPTIMISE_SPEED > 0 
        mutex_t  lock;          /* access to MDIO */
        unsigned last_time;
#endif
    } phy;
	unsigned long intr;		/* interrupt counter */
	unsigned char rxbuf_data [ETH_MTU + 36] __attribute__ ((aligned(8)));
	unsigned char txbuf_data [ETH_MTU + 36] __attribute__ ((aligned(8)));
	unsigned char *rxbuf;		/* aligned rxbuf[] */
	unsigned char *txbuf;		/* aligned txbuf[] */
	unsigned rxbuf_physaddr;	/* phys address of rxbuf[] */
	unsigned txbuf_physaddr;	/* phys address of txbuf[] */

#if ETH_OPTIMISE_SPEED > 0 
    mutex_t rx_lock;        /* get rx interrupts here */
    struct {
        mutex_t  lock;        /* get rx interrupts here */
        buf_t*   buf;
        unsigned byf_phys;
    } dma_rx;

	struct {
        //buf_t*   buf;
        unsigned byf_phys;
#if (ETH_TX_USE_DMA_IRQ > 0)
        mutex_t  lock;        /* get tx dma interrupts here */
#endif
#if ETH_TX_CHUNKS > 0
	    unsigned             task_physaddr;    /* phys address of txbuf[] */
        EMAC_PortCh_Settings emac_task[ETH_TX_CHUNKS] __attribute__ ((aligned(8)));
#endif
	} dma_tx;
#endif

	ARRAY (stack, ETH_STACKSZ);	/* stack for receive task */
	ARRAY (tstack, ETH_STACKSZ);	/* stack for transmit task */
} eth_t;

//
// Инициализация драйвера Ethernet
//
//	u		указатель на структуру, описывающую интерфейс
//	name		символическое имя интерфейса
//	prio		приоритет задачи-обработчика прерываний по выдаче
//			Приоритет задачи-обработчика прерываний по приёму
//			принимается равным (prio + 1)
//	pool		указатель на пул динамической памяти, используемый
//			драйвером Ethernet
//	arp		указатель на структуру ARP
//	macaddr		указатель на массив 6 байтов, содержащий MAC-адрес
//			устройства
//
void eth_init (eth_t *u, const char *name, int prio,
	struct _mem_pool_t *pool, struct _arp_t *arp, const unsigned char *macaddr);
	
//
// Вывод отладочной информации по драйверу Ethernet
//
//	u		указатель на структуру, описывающую интерфейс
//	stream		указатель на поток, в который выдавать отладочную
//			информацию
//
void eth_debug (eth_t *u, struct _stream_t *stream);

//
// Выполнить определение параметров сети
//
//	u		указатель на структуру, описывающую интерфейс
//
void eth_start_negotiation (eth_t *u);

//
// Определить наличие несущей в линии
//
//	u		указатель на структуру, описывающую интерфейс
//
int eth_get_carrier (eth_t *u);

//
// Определить текущие настройки скорости
//
//	u		указатель на структуру, описывающую интерфейс
//	duplex		указатель на переменную, в которую будет помещен
//			признак дуплексного режима
//	возвращает скорость в бит/сек
long eth_get_speed (eth_t *u, int *duplex);

//
// Установить внутреннюю петлю для самотестирования устройства
//
//	u		указатель на структуру, описывающую интерфейс
//	on		1 - установить петлю, 0 - отключить петлю
//
void eth_set_loop (eth_t *u, int on);

//
// Установить параметры режима прослушивания сети
//
//	u		указатель на структуру, описывающую интерфейс
//	station		узел
//	group		группа
void eth_set_promisc (eth_t *u, int station, int group);

//
// Опросить устройство
//
//	u		указатель на структуру, описывающую интерфейс
//
void eth_poll (eth_t *u);


void        eth_phy_write (eth_t *u, unsigned address, unsigned data);
unsigned    eth_phy_read (eth_t *u, unsigned address);
//*   опросить статус PHY 
//*   таймаут ETH_PHY_STASTUS_TOus задает время устаревания PHY_STASTUS, 
//    после которого eth_phy_poll форсирует запрос статуса с блокированием нитки 
void        eth_phy_poll(eth_t *u);
//\return != если есть соединение
unsigned    eth_phy_link_online(eth_t *u);



#ifdef __cplusplus
}
#endif

#endif /* __NVCOM_ETH_H */
