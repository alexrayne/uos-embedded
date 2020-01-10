/*
 * Ethernet driver for Elvees NVCom.
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
 */

#if defined(ELVEES_NVCOM01) || defined(ELVEES_NVCOM02T) || defined(ELVEES_NVCOM02) || defined(ELVEES_MC30SF6)

#include <runtime/lib.h>
#include <kernel/uos.h>
#include <kernel/internal.h>
#include <stream/stream.h>
#include <mem/mem.h>
#include <buf/buf.h>
#include <elvees/eth.h>
#include <elvees/ks8721bl.h>

#ifndef ETH_PRINTF
#define ETH_PRINTF(...) debug_printf(__VA_ARGS__)
#endif

#ifdef DEBUG_NET_ETH
#define ETH_printf(...) ETH_PRINTF(__VA_ARGS__)
#define EDMA_printf(...) ETH_PRINTF(__VA_ARGS__)
#define ETH_printf2(...) ETH_PRINTF(__VA_ARGS__)
#define ETH_dump(...)
#else
#define ETH_printf(...)
#define ETH_printf2(...)
#define EDMA_printf(...)
#define ETH_dump(...)
#endif

#ifdef DEBUG_NET_ETH_FAIL
#define ETHFAIL_printf(...) ETH_PRINTF(__VA_ARGS__)
#else
#define ETHFAIL_printf(...)
#endif

#ifdef DEBUG_NET_PHY
#define PHY_printf(...) ETH_PRINTF(__VA_ARGS__)
#else
#define PHY_printf(...)
#endif

#define MC_EMAC_TX          1
#define MC_EMAC_RX          0

#define ETH_IRQ_RECEIVE		12	/* receive interrupt */
#define ETH_IRQ_TRANSMIT	13	/* transmit interrupt */
#define ETH_IRQ_DMA_RX		14	/* receive DMA interrupt */
#define ETH_IRQ_DMA_TX		15	/* transmit DMA interrupt */

//форсирую сброс DCACHE после чтения фрейма ДМА
#define ETH_FORCE_CHACHE_SYNC 0

/*
 * Map virtual address to physical address in FM mode.
 */
#define virt_to_phys(x) mips_virtual_addr_to_physical(x)

void
cache_invalidate_on_buf(const buf_t* p){
    unsigned kseg = ( ( (unsigned long) (void*) (p->payload)) >> 28);
    enum {
          kuseg0 = 0
        , kuseg1 = 1
        , kuseg2 = 2
        , kuseg3 = 3
        , kseg0  = 4
        , kseg1  = 5
        , kseg_nocached    = kseg1
        , kseg2  = 6
        , kseg3  = 7
    };
#if ( ((UOS_MIPS_NOCACHEBLE_KSEG23 <= 0) && (UOS_MIPS_NOUSE_KSEG23 <= 0))\
    || ( (UOS_MIPS_NOCACHEBLE_KSEG0 <= 0) ) \
    || ( (UOS_MIPS_NOCACHEBLE_KUSEG <= 0) ) \
    ) && defined(C0CONF_NOCACHED)
    unsigned c0 = mips_read_c0_register (C0_CONFIG);
    switch(kseg){
        case kseg_nocached: return;
#if ( (UOS_MIPS_NOCACHEBLE_KSEG0 <= 0) )
        case kseg0        :
            if (C0CONF_K0_FIELD(c0) == C0CONF_NOCACHED)
                return;
#endif
#if ( (UOS_MIPS_NOCACHEBLE_KSEG23 <= 0) && (UOS_MIPS_NOUSE_KSEG23 <= 0) )
        case kseg2      :
        case kseg3      :
            if (C0CONF_K23_FIELD(c0) == C0CONF_NOCACHED)
                return;
#endif
#if ( (UOS_MIPS_NOCACHEBLE_KUSEG <= 0) )
        case kuseg0     :
        case kuseg1     :
        case kuseg2     :
        case kuseg3     :
            if (C0CONF_KU_FIELD(c0) == C0CONF_NOCACHED)
                return;
#endif
        default : return;
    };
#else
#ifndef ENABLE_DCACHE
    return;
#endif
    if (kseg == kseg_nocached)
        return;
#endif
    MC_CSR = MC_CSR | MC_CSR_FLUSH_D;
}

#ifdef ETH_USE_YELDING_WAIT
#define eth_yield() task_yield()
#define eth_yield_scale 1
#else
#define eth_yield()
#define eth_yield_scale 100
#endif


#ifndef ETH_MDIO_KHZ
#define ETH_MDIO_KHZ 2500ul
#endif

/*
 * PHY register write
 */
#ifdef ELVEES_NVCOM01
unsigned eth_phy_wait_poll(eth_t *u)   { 
    unsigned status;
    const unsigned MDIO_CPUTO = (KHZ/ETH_MDIO_KHZ + 1)*66;
    unsigned now;
    unsigned start = mips_read_c0_register (C0_COUNT);
    /* Wait until the PHY write completes. */
    status = MC_MAC_MD_STATUS;
    while ((status & MD_STATUS_BUSY) != 0) {
        now = mips_read_c0_register (C0_COUNT);
        if ((now-start) > MDIO_CPUTO)
            break;
        task_yield();
        status = MC_MAC_MD_STATUS;
    };
    return status;
}
#else
unsigned eth_phy_wait_poll(eth_t *u)   { 
    unsigned status, i;
    /* Wait until the PHY write completes. */
    for (i=0; i<100000; ++i) {
        status = MC_MAC_MD_STATUS;
        if (! (status & MD_STATUS_BUSY))
            break;
    }
    return status;
}
#endif

#if ETH_OPTIMISE_SPEED > 0
static inline 
void phy_lock(eth_t *u)   {
    mutex_lock(&u->phy.lock);
}

static inline 
bool_t phy_trylock(eth_t *u)   {
    return mutex_trylock(&u->phy.lock);
}

static inline 
void phy_unlock(eth_t *u) { 
    mutex_unlock(&u->phy.lock);
}

static 
__attribute__((unused))
unsigned phy_wait(eth_t *u)    {
        return (unsigned)mutex_wait(&u->phy.lock);
}

//проверяет завершение операции на шине и сигналит если операция завершилась
static 
bool_t phy_check(eth_t *u)   {
    unsigned tmp = MC_MAC_MD_STATUS;

    if ((tmp & MD_STATUS_BUSY) != 0)
        return 0;

    if ((tmp & MD_STATUS_END_READ) != 0){
        const unsigned cmd_mask = MD_CONTROL_REG(~0) | MD_CONTROL_PHY(~0);
        unsigned tmp_cmd = tmp | (MC_MAC_MD_CONTROL & cmd_mask); 
        if ( ((tmp_cmd ^ u->phy.last_status) >> 16) == 0){
            u->phy.last_status = tmp_cmd;
        }
    }
    else if ((tmp & MD_STATUS_END_WRITE) == 0)
        return 0;

    if (mutex_is_wait(&u->phy.lock))
        mutex_signal(&u->phy.lock, (void*)tmp);
    return 1;
}

#else
#define phy_lock(u)
#define phy_trylock(u)  0
#define phy_unlock(u)
#define phy_wait(u)  etc_phy_wait_poll(u)

static 
bool_t phy_check(eth_t *u)   {
    unsigned tmp = MC_MAC_MD_STATUS;
    if (((tmp ^ u->phy.last_status) >> 16) == 0){
        u->phy.last_status = tmp;
    }
    if ((tmp & MD_STATUS_BUSY) == 0)
        return 1;
    return 0;
}

#endif

//запускает операцию MDIO
void eth_phy_io_post(eth_t *u, unsigned address, unsigned cmd_data)
{
    bool_t ready = phy_check(u);
    phy_lock(u);

    // wait current operation completes
    if (!ready)
    if ((MC_MAC_MD_STATUS & MD_STATUS_BUSY) != 0)
        eth_phy_wait_poll(u);

    /* Issue the command to PHY. */
    MC_MAC_MD_CONTROL = cmd_data |   /* операция MD_CONTROL_OP_WRITE/READ и данные */
        MD_CONTROL_PHY (u->phy.adr) |       /* адрес PHY */
        MD_CONTROL_REG (address);       /* адрес регистра PHY */
}

//блокирующий обмен по MDIO
unsigned eth_phy_io(eth_t *u, unsigned address, unsigned cmd_data){

    eth_phy_io_post(u, address, cmd_data);
    /* Wait until the PHY write completes. */
    unsigned res = eth_phy_wait_poll(u);
    phy_unlock(u);
    /*debug_printf ((status & MD_STATUS_BUSY) ?
        "phy_write(%d, 0x%02x, 0x%04x) TIMEOUT\n" :
        "phy_write(%d, 0x%02x, 0x%04x)\n", u->phy, address, data);*/
    return res;
}

void
eth_phy_write (eth_t *u, unsigned address, unsigned data)
{
#ifdef DEBUG_NET_PHY
    unsigned res = 
#endif
            eth_phy_io(u, address, MD_CONTROL_DATA (data) | MD_CONTROL_OP_WRITE);
    PHY_printf("PHY:now %x\n", res);
}

/*
 * PHY register read
 */
unsigned
eth_phy_read (eth_t *u, unsigned address)
{
    return MD_CONTROL_DATA(eth_phy_io(u, address, MD_CONTROL_OP_READ));
}

static inline 
void phy_write (eth_t *u, unsigned address, unsigned data)
{
    eth_phy_write(u, address, data);
}

static inline 
unsigned    phy_read (eth_t *u, unsigned address)
{
    return eth_phy_read(u, address);
}

bool_t phy_status_ask(eth_t* u){
    if(!phy_check(u))
        return 0;
    if (phy_trylock(u)){
        MC_MAC_MD_CONTROL = MD_CONTROL_OP_READ |    /* операция чтения */
            MD_CONTROL_PHY (u->phy.adr) |       /* адрес PHY */
            MD_CONTROL_REG (PHY_STS);       /* адрес регистра PHY */
#if ETH_OPTIMISE_SPEED > 0
        u->phy.last_time = mips_read_c0_register (C0_COUNT);
#endif
        phy_unlock(u);
        return 1;
    }
    return 0;
}

#ifndef ETH_PHY_STASTUS_TOus
#define ETH_PHY_STASTUS_TOus    10000ul
#endif

#if ETH_OPTIMISE_SPEED > 0
void eth_phy_poll(eth_t *u){
    unsigned now = mips_read_c0_register (C0_COUNT);
    if ((now - u->phy.last_time) > (ETH_PHY_STASTUS_TOus * (KHZ/1000)) ){
        if (phy_status_ask(u))
            return;
        eth_phy_io_post(u, PHY_STS, MD_CONTROL_OP_READ);
        u->phy.last_time = mips_read_c0_register (C0_COUNT);
        phy_unlock(u);
    } 
}
#else
void eth_phy_poll(eth_t *u){
    phy_status_ask(u);
}
#endif

unsigned eth_phy_link_online(eth_t *u)
{
    phy_check(u);
    return (u->phy.last_status & PHY_STS_LINK);
}

/*
 * Set default values to Ethernet controller registers.
 */

static void
chip_init (eth_t *u)
{
	/* Включение тактовой частоты EMAC */
	MC_CLKEN |= MC_CLKEN_EMAC;
	udelay (10);

    /* Тактовый сигнал MDC не должен превышать 2.5 МГц. */
	const unsigned mdio_div = ((KHZ + ETH_MDIO_KHZ-1)/ETH_MDIO_KHZ);
	MC_MAC_MD_MODE = MD_MODE_DIVIDER(mdio_div);

	/* Find a device address of PHY transceiver.
	 * Count down from 31 to 0, several times. */
	unsigned id, retry = 0;
	u->phy.adr = 31;
#ifndef DEBUG_SIMULATE
	for (;;) {
        unsigned tmp = phy_read (u, PHY_ID1);
        if (tmp != 0 && tmp != 0xffff) {
            id = tmp << 16; 
            tmp = phy_read (u, PHY_ID2);
            id |= tmp;
            if (tmp != 0 && id != 0xffff)
                break;
        }
		if (u->phy.adr > 0)
			u->phy.adr--;
		else {
			u->phy.adr = 31;
			retry++;
			if (retry > 3) {
				debug_printf ("eth_init: no PHY detected\n");
				uos_halt (0);
			}
		}
	}
#else
	id = PHY_ID_KS8721BL;
#endif
#ifndef NDEBUG
	debug_printf ("eth_init: transceiver 0x%08x:`%s' detected at address %d\n"
	        , id, 
		((id & PHY_ID_MASK) == PHY_ID_KS8721BL) ? "KS8721" : "Unknown",
		u->phy.adr);
#endif

	//подготовлю шаблон для поллинга статуса в phy_check
	u->phy.last_status = MD_CONTROL_OP_READ |   /* операция чтения */
                        MD_CONTROL_PHY (u->phy.adr) |
                        MD_CONTROL_REG (PHY_STS);

	
	/* Reset transceiver. */
	phy_write (u, PHY_CTL, PHY_CTL_RST);
	int count;
	for (count=10000; count>0; count--)
		if (! (phy_read (u, PHY_CTL) & PHY_CTL_RST))
			break;
	if (count == 0)
		debug_printf ("eth_init: PHY reset failed\n");
	phy_write (u, PHY_EXTCTL, PHY_EXTCTL_JABBER);

	/* Perform auto-negotiation. */
	phy_write (u, PHY_ADVRT, PHY_ADVRT_CSMA | PHY_ADVRT_10_HDX |
		PHY_ADVRT_10_FDX | PHY_ADVRT_100_HDX | PHY_ADVRT_100_FDX);
	phy_write (u, PHY_CTL, PHY_CTL_ANEG_EN | PHY_CTL_ANEG_RST);

	phy_status_ask(u);

	MC_CSR_EMAC(MC_EMAC_RX) = MC_DMA_CSR_IPD;
    MC_CSR_EMAC(MC_EMAC_TX) = MC_DMA_CSR_IPD;
	
	/* Reset TX and RX blocks and pointers */
	MC_MAC_CONTROL = MAC_CONTROL_CP_TX | MAC_CONTROL_RST_TX |
			 MAC_CONTROL_CP_RX | MAC_CONTROL_RST_RX;
	udelay (10);
	/*debug_printf ("MAC_CONTROL: 0x%08x\n", MC_MAC_CONTROL);*/

	/* Общие режимы. */
	MC_MAC_CONTROL =
		MAC_CONTROL_FULLD |		/* дуплексный режим */
		MAC_CONTROL_EN_TX |		/* разрешение передачи */
		MAC_CONTROL_EN_TX_DMA |		/* разрешение передающего DMА */
		MAC_CONTROL_EN_RX |		/* разрешение приема */
		MAC_CONTROL_IRQ_TX_DONE | 	/* прерывание от передачи */
		MAC_CONTROL_IRQ_RX_DONE | 	/* прерывание по приёму */
		MAC_CONTROL_IRQ_RX_OVF; 	/* прерывание по переполнению */
	/*debug_printf ("MAC_CONTROL: 0x%08x\n", MC_MAC_CONTROL);*/

	/* Режимы приёма. */
	MC_MAC_RX_FRAME_CONTROL =
		RX_FRAME_CONTROL_DIS_RCV_FCS | 	/* не сохранять контрольную сумму */
		RX_FRAME_CONTROL_ACC_TOOSHORT |	/* прием коротких кадров */
		RX_FRAME_CONTROL_DIS_TOOLONG | 	/* отбрасывание слишком длинных кадров */
		RX_FRAME_CONTROL_DIS_FCSCHERR |	/* отбрасывание кадров с ошибкой контрольной суммы */
		RX_FRAME_CONTROL_DIS_LENGTHERR;	/* отбрасывание кадров с ошибкой длины */
	/*debug_printf ("RX_FRAME_CONTROL: 0x%08x\n", MC_MAC_RX_FRAME_CONTROL);*/

	/* Режимы передачи:
	 * запрет формирования кадра в блоке передачи. */
	MC_MAC_TX_FRAME_CONTROL = TX_FRAME_CONTROL_DISENCAPFR;
	/*debug_printf ("TX_FRAME_CONTROL: 0x%08x\n", MC_MAC_TX_FRAME_CONTROL);*/

	/* Режимы обработки коллизии. */
	MC_MAC_IFS_COLL_MODE = IFS_COLL_MODE_ATTEMPT_NUM(15) |
		IFS_COLL_MODE_EN_CW |
		IFS_COLL_MODE_COLL_WIN(64) |
		IFS_COLL_MODE_JAMB(0xC3) |
		IFS_COLL_MODE_IFS(24);

	/* Свой адрес. */
	MC_MAC_UCADDR_L = u->netif.ethaddr.val.l;
	MC_MAC_UCADDR_H = u->netif.ethaddr.val.h;
/*debug_printf ("UCADDR=%02x:%08x\n", MC_MAC_UCADDR_H, MC_MAC_UCADDR_L);*/

	/* Максимальный размер кадра. */
	MC_MAC_RX_FR_MAXSIZE = ETH_MTU;
}

void
eth_debug (eth_t *u, struct _stream_t *stream)
{
	unsigned short ctl, advrt, sts, extctl;

	ctl = phy_read (u, PHY_CTL);
	sts = phy_read (u, PHY_STS);
	advrt = phy_read (u, PHY_ADVRT);
	extctl = phy_read (u, PHY_EXTCTL);
	/*unsigned status_rx = MC_MAC_STATUS_RX;*/
	/*unsigned status_tx = MC_MAC_STATUS_TX;*/

	printf (stream, "CTL=%b\n", ctl, PHY_CTL_BITS);
	printf (stream, "STS=%b\n", sts, PHY_STS_BITS);
	printf (stream, "ADVRT=%b\n", advrt, PHY_ADVRT_BITS);
	printf (stream, "EXTCTL=%b\n", extctl, PHY_EXTCTL_BITS);
	/*printf (stream, "STATUS_TX=%b\n", status_tx, STATUS_TX_BITS);*/
	/*printf (stream, "STATUS_RX=%b\n", status_rx, STATUS_RX_BITS);*/
}

void eth_start_negotiation (eth_t *u)
{
	mutex_lock (&u->netif.lock);
	phy_write (u, PHY_ADVRT, PHY_ADVRT_CSMA | PHY_ADVRT_10_HDX |
		PHY_ADVRT_10_FDX | PHY_ADVRT_100_HDX | PHY_ADVRT_100_FDX);
	phy_write (u, PHY_CTL, PHY_CTL_ANEG_EN | PHY_CTL_ANEG_RST);
	mutex_unlock (&u->netif.lock);
}

int eth_get_carrier (eth_t *u)
{
	unsigned status;

	status = phy_read (u, PHY_STS);

	return (status & PHY_STS_LINK) != 0;
}

long eth_get_speed (eth_t *u, int *duplex)
{
	unsigned extctl;

	extctl = phy_read (u, PHY_EXTCTL);

	switch (extctl & PHY_EXTCTL_MODE_MASK) {
	case PHY_EXTCTL_MODE_10_HDX:	/* 10Base-T half duplex */
		if (duplex)
			*duplex = 0;
		u->netif.bps = 10 * 1000000;
		break;
	case PHY_EXTCTL_MODE_100_HDX:	/* 100Base-TX half duplex */
		if (duplex)
			*duplex = 0;
		u->netif.bps = 100 * 1000000;
		break;
	case PHY_EXTCTL_MODE_10_FDX:	/* 10Base-T full duplex */
		if (duplex)
			*duplex = 1;
		u->netif.bps = 10 * 1000000;
		break;
	case PHY_EXTCTL_MODE_100_FDX:	/* 100Base-TX full duplex */
		if (duplex)
			*duplex = 1;
		u->netif.bps = 100 * 1000000;
		break;
	default:
		return 0;
	}
	return u->netif.bps;
}

void eth_set_loop (eth_t *u, int on)
{
	unsigned control;

	mutex_lock (&u->netif.lock);

	/* Set PHY loop-back mode. */
	control = phy_read (u, PHY_CTL);
	if (on) {
		control |= PHY_CTL_LPBK;
	} else {
		control &= ~PHY_CTL_LPBK;
	}
	phy_write (u, PHY_CTL, control);
	mutex_unlock (&u->netif.lock);
}

void eth_set_promisc (eth_t *u, int station, int group)
{
	mutex_lock (&u->netif.lock);
	unsigned rx_frame_control = MC_MAC_RX_FRAME_CONTROL &
		~(RX_FRAME_CONTROL_EN_MCM | RX_FRAME_CONTROL_EN_ALL);
	if (station) {
		/* Accept any unicast. */
		rx_frame_control |= RX_FRAME_CONTROL_EN_ALL;
	} else if (group) {
		/* Accept any multicast. */
		rx_frame_control |= RX_FRAME_CONTROL_EN_MCM;
	}
	MC_MAC_RX_FRAME_CONTROL = rx_frame_control;
	mutex_unlock (&u->netif.lock);
}

/*
 * Put data to transmit FIFO from dma buffer.
 */
#if ETH_TX_CHUNKS > 0
//1 - если добавлена цепь 
bool_t eth_txfifo_chainset(eth_t* u, unsigned csr_tpl){
    EMAC_PortCh_Settings* task = u->dma_tx.emac_task;
    if (task[0].ir == 0){
        MC_CP_EMAC(MC_EMAC_TX) = 0;
        return 0;
    }

    csr_tpl |= MC_DMA_CSR_CHEN;
    unsigned i;
    for (i = 0; i < ETH_TX_CHUNKS-1; i++, task++){
        if (task[0].ir == 0){
            task[-1].csr &= ~MC_DMA_CSR_CHEN;
            break;
        }
        task[0].csr |= csr_tpl;
        EDMA_printf("<t:chain%x - %x %x>", task, task[0].ir, task[0].csr);
    }
    MC_CP_EMAC(MC_EMAC_TX) = u->dma_tx.task_physaddr;
    return 1;
}
#endif

#if ETH_TX_USE_DMA_IRQ > 0
#define eth_tx_lock(u)      mutex_lock(&((u)->dma_tx.lock))
#define eth_tx_unlock(u)    mutex_unlock(&((u)->dma_tx.lock))
#else
#define eth_tx_lock(u)
#define eth_tx_unlock(u)
#endif

static void
chip_write_txfifo (eth_t* u, unsigned physaddr, unsigned nbytes)
{
/*debug_printf ("write_txfifo %08x, %d bytes\n", physaddr, nbytes);*/
    /* Set the address and length for DMA. */
    unsigned csr = MC_DMA_CSR_WN(15) |
#if defined(ELVEES_NVCOM02T) || defined (ELVEES_NVCOM02)
		MC_DMA_CSR_WCX (nbytes - 1);
#else
		MC_DMA_CSR_WCX (((nbytes + 7) >> 3) - 1);
#endif
	MC_IR_EMAC(MC_EMAC_TX) = physaddr;
	MC_CP_EMAC(MC_EMAC_TX) = 0;
	MC_CSR_EMAC(MC_EMAC_TX) = csr;
    u->dma_tx.byf_phys = physaddr;
    EDMA_printf("<t%d> ", nbytes);

#if ETH_TX_CHUNKS > 0
	if (eth_txfifo_chainset(u, csr | MC_DMA_CSR_RUN)){
	    csr |= MC_DMA_CSR_CHEN;
	}
#endif

#if ETH_TX_USE_DMA_IRQ > 0

#ifdef ETH_DMA_WAITPOLL_TH
    if (nbytes < ETH_DMA_WAITPOLL_TH)
    {
        /* Run the DMA. */
        MC_CSR_EMAC(MC_EMAC_TX) = csr | MC_DMA_CSR_RUN;
        unsigned count;
        for (count=10000; count>0; count--) {
            csr = MC_CSR_EMAC(MC_EMAC_TX);
            if (! (csr & MC_DMA_CSR_RUN))
                break;
            //eth_yield();
            //debug_printf ("~");
        }
        if ((csr & (MC_DMA_CSR_RUN)) == 0)
            //при использовании прерывания ДМА позаботиться о старте передачи надо тут
            MC_MAC_TX_FRAME_CONTROL |= TX_FRAME_CONTROL_TX_REQ;
        else
            EDMA_printf("<!%08x>", csr);
    }
    else
#endif
    {
        eth_tx_lock(u);
        arch_intr_allow (ETH_IRQ_DMA_TX);
        MC_CSR_EMAC(MC_EMAC_TX) = csr | MC_DMA_CSR_IM | MC_DMA_CSR_RUN; //MC_DMA_CSR_IPD |;
        /* Run the DMA. */
        mutex_wait(&u->dma_tx.lock);
        //debug_putchar(0,'_');
        eth_tx_unlock(u);
    }

#else //ETH_TX_USE_DMA_IRQ
    {
        /* Run the DMA. */
        MC_CSR_EMAC(MC_EMAC_TX) = csr | MC_DMA_CSR_RUN;
        unsigned count;
        for (count=100000; count>0; count--) {
            csr = MC_CSR_EMAC(MC_EMAC_TX);
            if (! (csr & MC_DMA_CSR_RUN))
                break;
            //eth_yield();
            //debug_printf ("~");
        }
    }
#endif

    csr = MC_CSR_EMAC(MC_EMAC_TX);
    //bool_t done = (csr & (MC_DMA_CSR_END|MC_DMA_CSR_DONE)) != 0;
    bool_t done = (csr & (MC_DMA_CSR_RUN)) == 0;
    if (done){
        u->dma_tx.byf_phys = 0;
        EDMA_printf ("eth: TX DMA done, CSR=%08x, IR%x, TFS=%x\n"
                    , csr, MC_IR_EMAC(MC_EMAC_TX), MC_MAC_STATUS_TX);
    } 
    else{
        EDMA_printf ("eth: TX DMA failed, CSR=%08x\n", csr);
        MC_CSR_EMAC(MC_EMAC_TX) = 0;
    }
}


#if ETH_TX_USE_DMA_IRQ > 0
CODE_ISR
bool_t eth_tx_on_emac(void* data){
    eth_t* u = (eth_t*)data;

    unsigned csr = MC_CSR_EMAC(MC_EMAC_TX);
    //debug_putchar(0,'!');
    bool_t done = (csr & MC_DMA_CSR_RUN) == 0;
    if (done){
        MC_MAC_TX_FRAME_CONTROL |= TX_FRAME_CONTROL_TX_REQ;
        u->dma_tx.byf_phys = 0;
    }
    MC_CSR_EMAC(MC_EMAC_TX) = 0;
    return 0;
}
#endif

/*
 * Fetch data from receive FIFO to dma buffer.
 */
static void
__attribute__((unused)) 
chip_read_rxfifo (unsigned physaddr, unsigned nbytes)
{
	/* Set the address and length for DMA. */
	unsigned csr = MC_DMA_CSR_WN(15) |
#if defined(ELVEES_NVCOM02T) || defined (ELVEES_NVCOM02)
		MC_DMA_CSR_WCX (nbytes - 1);
#else
		MC_DMA_CSR_WCX (((nbytes + 7) >> 3) - 1);
#endif
	MC_CSR_EMAC(MC_EMAC_RX) = csr;
	MC_IR_EMAC(MC_EMAC_RX) = physaddr;
	MC_CP_EMAC(MC_EMAC_RX) = 0;
/*debug_printf ("(r%d) ", nbytes);*/

	/* Run the DMA. */
	MC_CSR_EMAC(MC_EMAC_RX) = csr | MC_DMA_CSR_RUN;

	unsigned count;
	for (count=100000; count>0; count--) {
		csr = MC_CSR_EMAC(MC_EMAC_RX);
		if (! (csr & MC_DMA_CSR_RUN))
			break;
	}
#ifdef ENABLE_DCACHE
    MC_CSR |= MC_CSR_FLUSH_D;
#endif
	if (count == 0) {
	    EDMA_printf ("eth: RX DMA failed, CSR=%08x\n", csr);
		MC_CSR_EMAC(MC_EMAC_RX) = 0;
	}
}

/*
 * Fetch data from receive FIFO to dma buffer.
 */
static void
chip_start_rxfifo (eth_t* u, unsigned nbytes)
{
    /* Set the address and length for DMA. */
    unsigned physaddr = u->dma_rx.byf_phys;

    unsigned csr = MC_DMA_CSR_WN(15)
                 | MC_DMA_CSR_IPD
                 | MC_DMA_CSR_WCX (nbytes - 1)
                 ;
    MC_CSR_EMAC(MC_EMAC_RX) = csr;
    MC_IR_EMAC(MC_EMAC_RX) = physaddr;
    MC_CP_EMAC(MC_EMAC_RX) = 0;
    EDMA_printf ("(r%d) ", nbytes);

    /* Run the DMA. */
    MC_CSR_EMAC(MC_EMAC_RX) = csr | MC_DMA_CSR_IM | MC_DMA_CSR_RUN;
}

static bool_t
chip_wait_rxfifo (eth_t* u)
{
    bool_t done = 0;
    unsigned csr;

#ifdef ETH_DMA_WAITPOLL_TH
    csr = MC_CSR_EMAC(MC_EMAC_RX);
    const unsigned lenmask = MC_DMA_CSR_WCX (~0);
    const unsigned th      = MC_DMA_CSR_WCX (ETH_DMA_WAITPOLL_TH);
    if ( (csr & lenmask) < th) {
        //для маленьких объемов данных не стану тратить время на переключения контекста в mutex_wait
        unsigned count;
        for (count=100000; count>0; count--) {
            csr = MC_CSR_EMAC(MC_EMAC_RX);
            if (! (csr & MC_DMA_CSR_RUN))
                break;
        }
    }
#endif

    csr = MC_CSR_EMAC(MC_EMAC_RX);
    done = (csr & MC_DMA_CSR_RUN) == 0;

    if (!done) {
        mutex_wait(&u->dma_rx.lock);
        csr = MC_CSR_EMAC(MC_EMAC_RX);
        EDMA_printf (" ->s(%x)\n",csr);
        done = (csr & MC_DMA_CSR_RUN) == 0;
    }

#if defined(ENABLE_DCACHE) && (ETH_FORCE_CHACHE_SYNC > 0)
    //сброс кеша делаю в eth_receive_frame
    MC_CSR = MC_CSR| MC_CSR_FLUSH_D;
#endif
    if (!done) {
        ETHFAIL_printf ("eth: RX DMA failed, CSR=%08x\n", csr);
        MC_CSR_EMAC(MC_EMAC_RX) = 0;
    }
    return done;
}

static void
chip_poll_rxfifo (eth_t* u){
    unsigned csr;
    mutex_lock (&u->dma_rx.lock);
    csr = MC_CSR_EMAC(MC_EMAC_RX);
    bool_t done = (csr & MC_DMA_CSR_RUN) == 0;
    if ((u->dma_rx.byf_phys != 0) & done)
        mutex_signal(&u->dma_rx.lock, u);
    mutex_unlock (&u->dma_rx.lock);
}

/*
 * Write the packet to chip memory and start transmission.
 * Deallocate the packet.
 */
static void
chip_transmit_packet (eth_t *u, buf_t *p)
{
	/* Send the data from the buf chain to the interface,
	 * one buf at a time. The size of the data in each
	 * buf is kept in the ->len variable. */
	buf_t *q;

    ETH_dump ("eth_send_data: length %d bytes %#12.6D %#2D\n"
                , p->tot_len
                , p->payload, p->payload+12
                );

    //запрошу статус PHY, за время отсылки пакета, возможно успеем считать его, и 
    //  по окончании отсылки фрейма вероятно поимеем актуальный статус 
    phy_status_ask(u);

    unsigned phys_buf = 0;
    unsigned phys_len = p->tot_len;
    unsigned len = phys_len;
#if (ETH_OPTIMISE_SPEED > 0) && (defined(ELVEES_NVCOM02T) || defined (ELVEES_NVCOM02))
    if (p->tot_len >= 60) {
        //в режиме TX_FRAME_CONTROL_DISENCAPFR, МАК не умеет добавлять PADы к короткому пакету
        //if ((ETH_TX_CHUNKS > 0) || (p->next == 0)) 
        {
            phys_buf = virt_to_phys((unsigned)(p->payload));
        }
        const unsigned misalign_2pass_th = 0xc0;
    #if ETH_TX_CHUNKS > 0
        if ((phys_buf & 4) == 0) 
        if (p->tot_len >= misalign_2pass_th) 
        {
            unsigned i = 0;
            unsigned sub = 0;
            EMAC_PortCh_Settings* task = u->dma_tx.emac_task;
            {
                q = p->next;
                while (q != 0) {
                    if (i >= ETH_TX_CHUNKS) break;
                    /* Copy the packet into the transmit buffer. */
                    assert (q->len > 0);
            /*debug_printf ("txcpy %08x <- %08x, %d bytes\n", buf, q->payload, q->len);*/
                    EMAC_PortCh_Settings* task = &(u->dma_tx.emac_task[i]);
                    unsigned qphy = virt_to_phys((unsigned)(q->payload));
                    if ((qphy & 4) == 0) {
                        task->ir =  qphy;
                        task->csr = MC_DMA_CSR_WCX (q->len - 1);
                    }
                    else if (i < ETH_TX_CHUNKS-1){
                        phys_buf = 0;
                        break;
                    }
                    else{
                        phys_buf = 0;
                        break;
                    }
                    ETH_printf2("chain%d:%08x, %x\n", i, task->ir, task->csr);
                    i++;
                    q=q->next;
                }
                if (q == 0){
                    if (i < ETH_TX_CHUNKS)
                        u->dma_tx.emac_task[i].ir = 0;
                }
                else{
                    //много чанков, потому придется их сложить вручную
                    u->dma_tx.emac_task[0].ir = 0;
                    phys_buf = 0;
                }
            }

            if (phys_buf == 0){
                u->dma_tx.emac_task[0].ir = 0;
            }
        } //if (p->tot_len >= 0x200)
    #else
        //DMA TX ELVEES_NVCOM02T глючит при работе с адресами невыравнеными на 8:
        //  учтем что он не должен стартовать с адреса +4 - ибо тогда слово +0 - будет всегда
        //      копией слова +4
        if ((phys_buf & 4) == 0) ;
        else {
            if (p->tot_len >= misalign_2pass_th) 
            {
                unsigned nextphy = (phys_buf|7)+1 +16;
                unsigned sub = nextphy - phys_buf;
                memcpy (u->txbuf, p->payload, sub);
                chip_write_txfifo (u, u->txbuf_physaddr, sub);
                phys_buf = nextphy;
                phys_len -= sub;
            }
            else
                phys_buf = 0;
        }
    #endif //ETH_TX_CHUNKS > 0
    }
#endif //#if ETH_OPTIMISE_SPEED > 0

    if (phys_buf == 0) {
    unsigned char *buf = (unsigned char*) u->txbuf;
    phys_buf = u->txbuf_physaddr;
    for (q=p; q; q=q->next) {
        /* Copy the packet into the transmit buffer. */
        assert (q->len > 0);
        ETH_printf2("txcpy %08x <- %08x, %d bytes\n", buf, q->payload, q->len);
        memcpy (buf, q->payload, q->len);
        buf += q->len;
    }
    
    len = p->tot_len;
    if (len < 60) {
        len = 60;
        phys_len = len;
        ETH_printf2 ("txzero %08x, %d bytes\n", u->txbuf + p->tot_len, len - p->tot_len);
        memset (u->txbuf + p->tot_len, 0, len - p->tot_len);

    }

    }//if (buf != 0)

    MC_MAC_TX_FRAME_CONTROL = TX_FRAME_CONTROL_DISENCAPFR |
        TX_FRAME_CONTROL_DISPAD |
        TX_FRAME_CONTROL_LENGTH (len);

    chip_write_txfifo (u, phys_buf, phys_len);

#if ETH_OPTIMISE_SPEED > 0
#    if !defined(ETH_TX_USE_DMA_IRQ) || (ETH_TX_USE_DMA_IRQ == 0)
        //если используем обработчик прерывания ЕМАК, то старт передачи находится в нем.
        MC_MAC_TX_FRAME_CONTROL |= TX_FRAME_CONTROL_TX_REQ;
#    endif

        if (u->dma_tx.byf_phys == 0){
            ++u->netif.out_packets;
            u->netif.out_bytes += len;
        }
        else {
            //debug_putchar(0,'/');
            //по какойто причине буфер не загрузили в ФИФО трансмитера
            u->netif.out_errors++;
            u->netif.out_discards++;
        }
#else //ETH_OPTIMISE_SPEED > 0
        MC_MAC_TX_FRAME_CONTROL |= TX_FRAME_CONTROL_TX_REQ;
        ++u->netif.out_packets;
        u->netif.out_bytes += len;
#endif

/*debug_printf ("tx%d", len); buf_print_data (u->txbuf, p->tot_len);*/
}

/*
 * Do the actual transmission of the packet. The packet is contained
 * in the pbuf that is passed to the function. This pbuf might be chained.
 * Return 1 when the packet is succesfully queued for transmission.
 * Or return 0 if the packet is lost.
 */
static bool_t
eth_output (eth_t *u, buf_t *p, small_uint_t prio)
{
    assert (p != 0);
	mutex_lock (&u->tx_lock);

	/* Exit if link has failed */
	if (p->tot_len < 4 || p->tot_len > ETH_MTU ||
	    ! eth_phy_link_online(u)
	    ) 
	{
		++u->netif.out_errors;
		mutex_unlock (&u->tx_lock);
		ETHFAIL_printf("eth_output: transmit %d bytes, link failed\n", p->tot_len);
		netif_free_buf (&u->netif, p);
		return 0;
	}

    bool_t is_full  = buf_queueh_is_full  (&u->outq);
    netif_io_overlap* over = netif_is_overlaped(p);
    if (over != 0){
        over->asynco = nios_inprocess;
        unsigned ops = over->options;
        if ( (ops & nioo_ActionTerminate) == 0)
        {
        unsigned qlimit = ops & nioo_FreeLevel;
        if (qlimit == 0)
            qlimit = ETH_OUTQ_SIZE - ETH_OUTQ_PRESERVE;
        else
            qlimit = ETH_OUTQ_SIZE - qlimit;
        is_full = qlimit <= u->outq.count;
        }
        else
            is_full = 1;
        //пакет с оверлеем не ожидает очереди, потому что он умеет самообслуживаться
        //  потому сразу отказываем ему если он не помещается в очередь
        if (is_full) {
            mutex_unlock (&u->tx_lock);
            netif_free_buf (&u->netif, p);
            return 0;
        }
    }

	/* Занято, ставим в очередь. */
    #if 0
    if (buf_queue_is_full (&u->outq)) {
        /* Нет места в очереди: теряем пакет. */
        ++u->netif.out_discards;
        mutex_unlock (&u->tx_lock);
        debug_printf ("eth_output: overflow\n");
        netif_free_buf (&u->netif, p);
        return 0;
    }
    #else
    ETH_printf ("eth_output: tx que %d bytes\n", p->tot_len);
    while (buf_queueh_is_full (&u->outq)) {
        mutex_wait (&u->tx_lock);
    }
    #endif

    unsigned status_tx = MC_MAC_STATUS_TX;
    bool_t isbusy = ((status_tx & STATUS_TX_ONTX_REQ) != 0);
    bool_t is_clean = buf_queueh_is_empty (&u->outq);

    if (!is_clean || (isbusy))
        buf_queueh_put (&u->outq, p);

    else {
	    ETH_printf ("eth_output:(tx status $%x) tx %d bytes\n", status_tx, p->tot_len);
		/* Смело отсылаем. */
		chip_transmit_packet (u, p);
		mutex_unlock (&u->tx_lock);
		netif_free_buf (&u->netif, p);
		return 1;
	}
	
	mutex_unlock (&u->tx_lock);
	return 1;
}

/*
 * Get a packet from input queue.
 */
static buf_t *
eth_input (eth_t *u)
{
	buf_t *p;
#if ETH_OPTIMISE_SPEED > 0
    mutex_t* const rx_lock = &u->rx_lock;
#else
    const mutex_t* rx_lock = &u->netif.lock;
#endif

	mutex_lock (rx_lock);
	p = buf_queueh_get (&u->inq);
	mutex_unlock (rx_lock);
	return p;
}

/*
 * Setup MAC address.
 */
static void
eth_set_address (eth_t *u, unsigned char *addr)
{
	mutex_lock (&u->netif.lock);
	u->netif.ethaddr = macadr_4ucs(addr);

	MC_MAC_UCADDR_L = u->netif.ethaddr.val.l;
	MC_MAC_UCADDR_H = u->netif.ethaddr.val.h;
/*debug_printf ("UCADDR=%02x:%08x\n", MC_MAC_UCADDR_H, MC_MAC_UCADDR_L);*/

	mutex_unlock (&u->netif.lock);
}

/*
 * Fetch the received packet from the network controller.
 * Put it to input queue.
 */
static void
eth_receive_frame (eth_t *u)
{
    if (u->dma_rx.byf_phys != 0){
        return;
    }

	unsigned frame_status = MC_MAC_RX_FRAME_STATUS_FIFO;
	if (! (frame_status & RX_FRAME_STATUS_OK)) {
		/* Invalid frame */
	    ETHFAIL_printf("eth_receive_data: failed, frame_status=%#08x\n", frame_status);
		++u->netif.in_errors;
		return;
	}

#if defined(ELVEES_NVCOM02T) || defined (ELVEES_NVCOM02)
	buf_t *p = 0;

	/* Extract data from RX FIFO. */
	unsigned len = RX_FRAME_STATUS_LEN (frame_status);

	if (len < 4 || len > ETH_MTU) 
	{
        /* Skip this frame */
	    ETHFAIL_printf("eth_receive_data: bad length %d bytes, frame_status=%#08x\n", len, frame_status);
        ++u->netif.in_errors;
    }
	else if (buf_queueh_is_full (&u->inq)) {
	        ETHFAIL_printf ("eth_receive_data: input overflow\n");
	        ++u->netif.in_discards;
	}
	else {
	    /* Allocate a buf chain with total length 'len' */
	    p = buf_alloc (u->pool, len, 2);
	    //ETH_printf("eth_receive_data:alloc $%x[%d] for len %d\n", p, mem_size(p), len);
	    if (! p) {
	        /* Could not allocate a buf - skip received frame */
	        ETHFAIL_printf("eth_receive_data: ignore packet - out of memory\n");
	        ++u->netif.in_discards;
	    }
	}

	unsigned buf = (p != 0)? virt_to_phys((unsigned)(p->payload) ) : u->rxbuf_physaddr;
#if ETH_OPTIMISE_SPEED > 0
	u->dma_rx.buf = p;
	u->dma_rx.byf_phys = buf;
    chip_start_rxfifo (u, len);
    if (chip_wait_rxfifo(u)) {
        ++u->netif.in_packets;
        u->netif.in_bytes += len;
        u->dma_rx.buf = 0;
        u->dma_rx.byf_phys = 0;
    }
    else {
        u->dma_rx.buf = 0;
        u->dma_rx.byf_phys = 0;
        buf_free(p);
        p = 0;
        ETHFAIL_printf("eth_recv_data: fifo lost\n");
    }
#else
    chip_read_rxfifo(buf, len);
#endif

    if (p != 0){
        buf_queueh_put (&u->inq, p);
#       if ETH_OPTIMISE_SPEED > 0
        mutex_signal(&u->netif.lock, u);
#       else
#       endif
        cache_invalidate_on_buf(p);
        ETH_printf ("eth_receive_data: ok, frame_status=%#08x, length %d bytes\n", frame_status, len);
        ETH_dump ("eth_receive_data: mac:%#12.6D msg:%#*.16D\n", p->payload, len-12, p->payload+12);
    }
//    else
//        debug_putchar(0, '#');

#else //defined(ELVEES_NVCOM02T) || defined (ELVEES_NVCOM02)
    /* Extract data from RX FIFO. */
    unsigned len = RX_FRAME_STATUS_LEN (frame_status);
#if ETH_OPTIMISE_SPEED > 0
    u->dma_rx.buf = 0;
    u->dma_rx.byf_phys = u->rxbuf_physaddr;
    chip_start_rxfifo (u, len);
    if (chip_wait_rxfifo(u)) {
        ++u->netif.in_packets;
        u->netif.in_bytes += len;
        u->dma_rx.buf = 0;
        u->dma_rx.byf_phys = 0;
    }
#else
    chip_read_rxfifo(u->rxbuf_physaddr, len);
#endif

    if (len < 4 || len > ETH_MTU) {
        /* Skip this frame */
        ETHFAIL_printf ("eth_receive_data: bad length %d bytes, frame_status=%#08x\n", len, frame_status);
        ++u->netif.in_errors;
        return;
    }
    ++u->netif.in_packets;
    u->netif.in_bytes += len;
    if (0)
    {
        unsigned char* buf = u->rxbuf;
        ETH_printf ("eth_receive_data: ok, frame_status=%#08x, length %d bytes\n", frame_status, len);
        ETH_printf ("eth_receive_data: %#12.6D %#2D\n", buf, buf+12);
    }

    if (buf_queue_is_full (&u->inq)) {
        ETHFAIL_printf ("eth_receive_data: input overflow\n");
        ++u->netif.in_discards;
        return;
    }

    /* Allocate a buf chain with total length 'len' */
    buf_t *p = buf_alloc (u->pool, len, 2);
    if (! p) {
        /* Could not allocate a buf - skip received frame */
        ETHFAIL_printf ("eth_receive_data: ignore packet - out of memory\n");
        ++u->netif.in_discards;
        return;
    }

    /* Copy the packet data. */
    memcpy (p->payload, u->rxbuf, len);
#   if ETH_OPTIMISE_SPEED > 0
    buf_queue_put (&u->inq, p);
    mutex_signal(&u->netif.lock, u);
#   else
    buf_queue_put (&u->inq, p);
#   endif
#endif
/*debug_printf ("[%d]", p->tot_len); buf_print_ethernet (p);*/
}

/*
 * Process a receive interrupt.
 * Return nonzero when there was some activity.
 */
static unsigned
handle_receive_interrupt (eth_t *u)
{
	unsigned active = 0;
	phy_status_ask(u);
	for (;;) {
		unsigned status_rx = MC_MAC_STATUS_RX;
/*debug_printf ("eth rx irq: STATUS_RX = %08x\n", status_rx);*/
		if (status_rx & (STATUS_RX_STATUS_OVF | STATUS_RX_FIFO_OVF)) {
			/* Count lost incoming packets. */
			if (STATUS_RX_NUM_MISSED (status_rx))
				u->netif.in_discards += STATUS_RX_NUM_MISSED (status_rx);
			else
				u->netif.in_discards++;
			MC_MAC_STATUS_RX = 0;
		}
	    ETH_printf("eth_recv status %x\n", status_rx);

		/* Check if a packet has been received and buffered. */
		if (! (status_rx & STATUS_RX_DONE)) {
			/* All interrupts processed. */
			return active;
		}
		++active;
		MC_MAC_STATUS_RX = 0;

		/* Fetch all received packets. */
		unsigned nframes = STATUS_RX_NUM_FR (status_rx);
		while (nframes-- > 0)
			eth_receive_frame (u);
	}
}

/*
 * Process a transmit interrupt: fast handler.
 * Return 1 when we are expecting the hardware interrupt.
 */
static void
handle_transmit_interrupt (eth_t *u)
{
    phy_check(u);
	unsigned status_tx = MC_MAC_STATUS_TX;
	if (status_tx & STATUS_TX_ONTX_REQ) {
		/* Передачик пока не закончил. */
	    ETH_printf2 ("eth tx irq: ONTX_REQ, STATUS_TX = %08x\n", status_tx);
		return;
	}
	MC_MAC_STATUS_TX = 0;
	//* возможно не стоит уведомлять о отсылке публичный интерфейс. может имеет
	//      смысл выделить для этого отдельный мутех
	mutex_signal (&u->netif.lock, 0);

	/* Подсчитываем коллизии. */
	if (status_tx & (STATUS_TX_ONCOL | STATUS_TX_LATE_COLL)) {
		++u->netif.out_collisions;
	}

	/* Извлекаем следующий пакет из очереди. */
	buf_t *p = buf_queueh_get (&u->outq);
	if (! p) {
	    ETH_printf2 ("eth tx irq: done, STATUS_TX = %08x\n", status_tx);
/*debug_printf ("#");*/
		return;
	}
	mutex_signal(&u->tx_lock, 0);

	/* Передаём следующий пакет. */
	ETH_printf2 ("eth tx irq: send next packet, STATUS_TX = %08x\n", status_tx);
	chip_transmit_packet (u, p);
	netif_free_buf (&u->netif, p);
}

/*
 * Poll for interrupts.
 * Must be called by user in case there is a chance to lose an interrupt.
 */
void
eth_poll (eth_t *u)
{
#if ETH_OPTIMISE_SPEED > 0
    mutex_t* const rx_lock = &u->rx_lock;
#else
    const mutex_t* rx_lock = &u->netif.lock;
#endif
	mutex_lock (rx_lock);
	if (handle_receive_interrupt (u)) {
		mutex_signal (&u->netif.lock, 0);
		chip_poll_rxfifo(u);
	}
	mutex_unlock (rx_lock);
	

	mutex_lock (&u->tx_lock);
//if (MC_MAC_STATUS_TX & STATUS_TX_DONE)
	handle_transmit_interrupt (u);
	mutex_unlock (&u->tx_lock);
}

/*
 * Receive interrupt task.
 */
static void
eth_receiver (void *arg)
{
	eth_t *u = (eth_t *)arg;
#if ETH_OPTIMISE_SPEED > 0
    mutex_t* const rx_lock = &u->rx_lock;
    mutex_lock_irq (&u->dma_rx.lock, ETH_IRQ_DMA_RX, 0, u);
#else
    const mutex_t* rx_lock = &u->netif.lock;
#endif

	/* Register receive interrupt. */
    //mutex_init(&u->rx_lock);
    mutex_lock_irq (rx_lock, ETH_IRQ_RECEIVE, 0, u);


	for (;;) {
		/* Wait for the receive interrupt. */
		mutex_wait (rx_lock);
		++u->intr;
		handle_receive_interrupt (u);
	}
}

/*
 * Trasmit interrupt task.
 */
static void
eth_transmitter (void *arg)
{
	eth_t *u = (eth_t *)arg;

	/* Register transmit interrupt. */
    //mutex_init(&u->tx_lock);
	mutex_lock_irq (&u->tx_lock, ETH_IRQ_TRANSMIT, 0, u);
#if (ETH_TX_USE_DMA_IRQ > 0)
    mutex_attach_irq (&u->dma_tx.lock, ETH_IRQ_DMA_TX, &(eth_tx_on_emac), u);
    //arch_intr_allow (ETH_IRQ_DMA_TX);
    //mutex_unlock(&u->dma_tx.lock);
#endif

	for (;;) {
		/* Wait for the transmit interrupt. */
		mutex_wait (&u->tx_lock);
		++u->intr;
		handle_transmit_interrupt (u);
	}
}

static netif_interface_t eth_interface = {
	(bool_t (*) (netif_t*, buf_t*, small_uint_t))
						eth_output,
	(buf_t *(*) (netif_t*))			eth_input,
	(void (*) (netif_t*, unsigned char*))	eth_set_address,
};

/*
 * Set up the network interface.
 */
void
eth_init (eth_t *u, const char *name, int prio, mem_pool_t *pool,
	struct _arp_t *arp, const unsigned char *macaddr)
{
	u->netif.interface = &eth_interface;
	u->netif.name = name;
	u->netif.arp = arp;
	u->netif.mtu = 1500;
	u->netif.type = NETIF_ETHERNET_CSMACD;
	u->netif.bps = 10000000;
	u->netif.ethaddr = macadr_4ucs(macaddr);

	u->pool = pool;
	u->rxbuf = (unsigned char*) (((unsigned) u->rxbuf_data + 7) & ~7);
	u->txbuf = (unsigned char*) (((unsigned) u->txbuf_data + 7) & ~7);
	u->rxbuf_physaddr = virt_to_phys ((unsigned) u->rxbuf);
	u->txbuf_physaddr = virt_to_phys ((unsigned) u->txbuf);
	buf_queueh_init (&u->inq, sizeof (u->inqdata));
	buf_queueh_init (&u->outq, sizeof (u->outqdata));
#if ETH_TX_CHUNKS > 0
	//создаю ДМА-цепь 
	u->dma_tx.task_physaddr = virt_to_phys((unsigned)(u->dma_tx.emac_task));
    EDMA_printf("\n<emac:chain0:%x>\n", u->dma_tx.task_physaddr);
	unsigned i;
	EMAC_PortCh_Settings* task = u->dma_tx.emac_task;
	for (i = 0; i < ETH_TX_CHUNKS-1; i ++, task++){
	    task[0].cp = (EMAC_PortCh_Settings*)virt_to_phys( (unsigned)(task+1) );
	    EDMA_printf("<emac:chain%d:%x>\n", i+1, task[0].cp);
	}
	u->dma_tx.emac_task[ETH_TX_CHUNKS-1].cp = (EMAC_PortCh_Settings*)0;
#endif
	/* Initialize hardware. */
	chip_init (u);

	/* Create transmit task. */
	task_create (eth_transmitter, u, "eth-tx", prio, u->tstack, sizeof (u->tstack));

	/* Create receive task. */
	task_create (eth_receiver, u, "eth-rx", prio+1, u->stack, sizeof (u->stack));
}

#endif
