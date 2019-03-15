/*
 * Ethernet driver for Elvees NVCom.
 * Copyright (c) 2010 Serge Vakulenko.
 *               2013 Dmitry Podkhvatilin
 * Based on sources from UOS elvees-mcb driver and examples from Milandr 
 * tech-team www.milandr.ru
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
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// Использование eth_err_0022_correction() после записи в eth-регистры требуется
// из-за возможного использования секции data_hi для динамической памяти (net_pool) и еth-буферов
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

#include <runtime/lib.h>
#include <kernel/uos.h>
#include <kernel/internal.h>
#include <stream/stream.h>
#include <mem/mem.h>
#include <buf/buf.h>

#include "ve1eth.h"

task_t *eth_task;
mutex_t eth_interrupt_mutex;
list_t  eth_interrupt_handlers;
uint8_t eth_interrupt_task_created = 0;

#if defined (ETH_USE_DMA)
mutex_t dma_interrupt_mutex, dma_mutex;
DMA_Data_t dma_conf[64]    __attribute__((aligned(1024)))   __attribute__((section(".data_hi")));
uint32_t eth_rx_buf[384]   __attribute__((section(".data_hi")));
uint32_t eth_tx_buf[384]   __attribute__((section(".data_hi")));
#elif defined (ETH_BUFF_DATA_HI)
uint32_t eth_rx_buf[384]   __attribute__((section(".data_hi")));
uint32_t eth_tx_buf[384]   __attribute__((section(".data_hi")));
#else
uint32_t eth_rx_buf[384];
uint32_t eth_tx_buf[384];
#endif

void eth_send_frame_dma(const void *buf, uint16_t frame_length);
void eth_send_frame(const void *buf, uint16_t frame_length);
uint32_t eth_read_frame_dma(void *buf);
uint32_t eth_read_frame(void *buf);
void eth_set_phyreg(uint8_t addr, uint8_t num, uint16_t data);
void eth_chip_transmit_packet (eth_t *u, buf_t *p);
uint16_t eth_get_phyreg(uint8_t addr, uint8_t num);
uint16_t eth_handle_receive_interrupt (eth_t *u);
void eth_handle_transmit_interrupt (eth_t *u);
void eth_interrupt_task(void *arg);
void eth_nops(void);

#ifndef ETH_CRS_LED
#   define ETH_CRS_LED PB14
#endif

#if (PORT(ETH_CRS_LED)==PORT_A)
#   define ETH_CRS_GPIO ARM_GPIOA
#elif (PORT(ETH_CRS_LED)==PORT_B)
#   define ETH_CRS_GPIO ARM_GPIOB
#elif (PORT(ETH_CRS_LED)==PORT_C)
#   define ETH_CRS_GPIO ARM_GPIOC
#elif (PORT(ETH_CRS_LED)==PORT_D)
#   define ETH_CRS_GPIO ARM_GPIOD
#else
#   error "ETH_CRS_LED pin is not assigned in CFLAGS of target.cfg"
#endif

#ifndef ETH_LINK_LED
#   define ETH_LINK_LED PB15
#endif

#if (PORT(ETH_LINK_LED)==PORT_A)
#   define ETH_LINK_GPIO ARM_GPIOA
#elif (PORT(ETH_LINK_LED)==PORT_B)
#   define ETH_LINK_GPIO ARM_GPIOB
#elif (PORT(ETH_LINK_LED)==PORT_C)
#   define ETH_LINK_GPIO ARM_GPIOC
#elif (PORT(ETH_LINK_LED)==PORT_D)
#   define ETH_LINK_GPIO ARM_GPIOD
#else
#   error "ETH_LINK_LED pin is not assigned in CFLAGS of target.cfg"
#endif

#ifndef ETH_SPEED_LED
#   define ETH_SPEED_LED PD14
#endif

#if (PORT(ETH_SPEED_LED)==PORT_A)
#   define ETH_SPEED_GPIO ARM_GPIOA
#elif (PORT(ETH_SPEED_LED)==PORT_B)
#   define ETH_SPEED_GPIO ARM_GPIOB
#elif (PORT(ETH_SPEED_LED)==PORT_C)
#   define ETH_SPEED_GPIO ARM_GPIOC
#elif (PORT(ETH_SPEED_LED)==PORT_D)
#   define ETH_SPEED_GPIO ARM_GPIOD
#else
#   error "ETH_SPEED_LED pin is not assigned in CFLAGS of target.cfg"
#endif

#ifndef ETH_HD_LED
#   define ETH_HD_LED PD13
#endif

#if (PORT(ETH_HD_LED)==PORT_A)
#   define ETH_HD_GPIO ARM_GPIOA
#elif (PORT(ETH_HD_LED)==PORT_B)
#   define ETH_HD_GPIO ARM_GPIOB
#elif (PORT(ETH_HD_LED)==PORT_C)
#   define ETH_HD_GPIO ARM_GPIOC
#elif (PORT(ETH_HD_LED)==PORT_D)
#   define ETH_HD_GPIO ARM_GPIOD
#else
#   error "ETH_HD_LED pin is not assigned in CFLAGS of target.cfg"
#endif

bool_t buf_queue_count (buf_queue_t *q) 
{
    return q->count;
}

inline __attribute__((always_inline)) uint32_t buf_queue_size (buf_queue_t *q)
{
    buf_t *p;
    uint32_t bq_len=0;
    struct _buf_t **q_tail = q->tail;
    struct _buf_t **q_queue = q->queue;
    small_uint_t q_size = q->size;
    small_uint_t q_count = q->count;

    while(q_count) {
        assert (q_tail >= q_queue);
        assert (q_tail < q_queue + q_size);

        if (q_count == 0) {
        //debug_printf ("buf_queue_get: returned 0\n");
            return 0;
        }
        assert (*q_tail != 0);

        // Get the first packet from queue. 
        p = *q_tail;
        bq_len += p->tot_len;
        //debug_printf ("bq_len+=len %d\n",bq_len);
        // Advance head pointer. 
        if (--q_tail < q_queue)
        q_tail += q_size;
        --q_count;
    }
    /*debug_printf ("buf_queue_get: returned 0x%04x\n", p);*/
    return 0;//bq_len;
}

// Индикация физического состояния линии, вызывается в задаче пользователя
void eth_led(void)
{
    /* Возможные варианты индикации 
    * ARM_ETH_PHY_LED_SPEED  10 или 100 Мбит
    * ARM_ETH_PHY_LED_LINK
    * ARM_ETH_PHY_LED_CRS    Наличие несущей
    * ARM_ETH_PHY_LED_HD     Дуплекс/полудуплекс
    */ 

    if(ETH_CRS_GPIO->DATA & (1 <<  PIN(ETH_CRS_LED))) {
        ETH_CRS_GPIO->CLRTX |= 1 << PIN(ETH_CRS_LED);
    } else {
        if(!(ARM_ETH->PHY_STAT & ARM_ETH_PHY_LED_CRS))
            ETH_CRS_GPIO->SETTX |= 1 << PIN(ETH_CRS_LED); // наличие
        else                                            
            ETH_CRS_GPIO->CLRTX |= 1 << PIN(ETH_CRS_LED); // отсутствие
    }

    // индикация наличия Link сигнала
    if(!(ARM_ETH->PHY_STAT & ARM_ETH_PHY_LED_LINK))
        ETH_LINK_GPIO->SETTX |= 1 << PIN(ETH_LINK_LED);   // наличие
    else
        ETH_LINK_GPIO->CLRTX |= 1 << PIN(ETH_LINK_LED);   // отсутствие

    // скорость 100 / 10 мбит
    if(!(ARM_ETH->PHY_STAT & ARM_ETH_PHY_LED_SPEED))
        ETH_SPEED_GPIO->SETTX |= 1 << PIN(ETH_SPEED_LED); // 100 mbit
    else
        ETH_SPEED_GPIO->CLRTX |= 1 << PIN(ETH_SPEED_LED); // 10 mbit

    // режим
    if(!(ARM_ETH->PHY_STAT & ARM_ETH_PHY_LED_HD))   
        ETH_HD_GPIO->SETTX |= 1 << PIN(ETH_HD_LED);       // дуплекс
    else
        ETH_HD_GPIO->CLRTX |= 1 << PIN(ETH_HD_LED);       // полудуплекс
}

void eth_set_HASH_table(eth_t *u, uint8_t *value)
{
 #ifndef ETH_POLL_MODE 
    mutex_lock (&eth_interrupt_mutex);
 #endif
    mutex_lock (&u->netif.lock);
    memcpy ((uint8_t*)ARM_ETH->HASH, value, 8);
    mutex_unlock (&u->netif.lock);
 #ifndef ETH_POLL_MODE
    mutex_unlock (&eth_interrupt_mutex);
 #endif
}

void eth_get_HASH_table (uint8_t *value)
{
    memcpy (value, (uint8_t*)ARM_ETH->HASH, 8);
}

void eth_set_MAC_addr (eth_t *u, uint8_t *value)
{
 #ifndef ETH_POLL_MODE 
    mutex_lock (&eth_interrupt_mutex);
 #endif	
    mutex_lock(&u->netif.lock);
    memcpy((uint8_t*)ARM_ETH->MAC_ADDR, value, 6);
    mutex_unlock(&u->netif.lock);
 #ifndef ETH_POLL_MODE
    mutex_unlock (&eth_interrupt_mutex);
 #endif	
}

void eth_get_MAC_addr (uint8_t *value)
{
    memcpy(value, (uint8_t*)ARM_ETH->MAC_ADDR, 6);
}

void eth_debug (eth_t *u, struct _stream_t *stream)
{
    mutex_lock(&u->netif.lock);
    uint16_t basic_ctrl = eth_get_phyreg(ARM_ETH_PHY_ADDRESS, 0);
    uint16_t basic_stat = eth_get_phyreg(ARM_ETH_PHY_ADDRESS, 1);
    uint16_t id_phy1 = eth_get_phyreg(ARM_ETH_PHY_ADDRESS, 2);
    uint16_t id_phy2 = eth_get_phyreg(ARM_ETH_PHY_ADDRESS, 3);
    uint16_t auto_adj = eth_get_phyreg(ARM_ETH_PHY_ADDRESS, 4);
    uint16_t opponent = eth_get_phyreg(ARM_ETH_PHY_ADDRESS, 5);
    uint16_t ext_adj = eth_get_phyreg(ARM_ETH_PHY_ADDRESS, 6);
    uint16_t ext_man = eth_get_phyreg(ARM_ETH_PHY_ADDRESS, 18);
    uint16_t irq_flg = eth_get_phyreg(ARM_ETH_PHY_ADDRESS, 29);
    uint16_t irq_mask= eth_get_phyreg(ARM_ETH_PHY_ADDRESS, 30);
    uint16_t ext_state = eth_get_phyreg(ARM_ETH_PHY_ADDRESS, 31);
    uint16_t g_cfg_hi = ARM_ETH->G_CFG_HI;
    uint16_t g_cfg_low= ARM_ETH->G_CFG_LOW;
    uint16_t x_cfg = ARM_ETH->X_CFG;
    uint16_t r_cfg = ARM_ETH->R_CFG;
    uint16_t imr = ARM_ETH->IMR;
    uint16_t ifr = ARM_ETH->IFR;
    uint16_t stat= ARM_ETH->STAT;
    uint16_t phy_ctrl = ARM_ETH->PHY_CTRL;
    uint16_t phy_stat = ARM_ETH->PHY_STAT;
    uint16_t rxbf_head = ARM_ETH->R_HEAD;
    uint16_t rxbf_tail = ARM_ETH->R_TAIL;
    uint16_t txbf_head = ARM_ETH->X_HEAD;
    uint16_t txbf_tail = ARM_ETH->X_TAIL;
    mutex_unlock (&u->netif.lock);

    printf (stream, "G_CFG_HI = 0x%04X\n", g_cfg_hi);
    printf (stream, "G_CFG_LOW= 0x%04X\n", g_cfg_low);
    printf (stream, "X_CFG = 0x%04X\n", x_cfg);
    printf (stream, "R_CFG = 0x%04X\n", r_cfg);
    printf (stream, "IMR = 0x%04X\n", imr);
    printf (stream, "IFR = 0x%04X\n", ifr);
    printf (stream, "STAT = 0x%04X\n", stat);
    printf(stream, "RXBF HEAD:TAIL = %04x:%04x\n", rxbf_head, rxbf_tail);
    printf(stream, "TXBF HEAD:TAIL = %04x:%04x\n", txbf_head, txbf_tail);
    printf (stream, "PHY_CTRL = 0x%04X\n", phy_ctrl);
    printf (stream, "PHY_STAT = 0x%04X\n", phy_stat);
    printf (stream, "PHY_BASIC_CTRL(0) = 0x%04X\n", basic_ctrl);
    printf (stream, "PHY_BASIC_STAT(1) = 0x%04X\n", basic_stat);
    printf (stream, "PHY_ID_1(2) = 0x%04X\n", id_phy1);
    printf (stream, "PHY_ID_2(3) = 0x%04X\n", id_phy2);
    printf (stream, "PHY_AUTO_ADJ(4) = 0x%04X\n", auto_adj);
    printf (stream, "PHY_OPPO_ADJ(5) = 0x%04X\n", opponent);
    printf (stream, "PHY_EXT_ADJ(6)  = 0x%04X\n", ext_adj);
    printf (stream, "PHY_EXT_MODE(18) = 0x%04X\n", ext_man);
    printf (stream, "PHY_IRQ_FLAGS(29)= 0x%04X\n", irq_flg);
    printf (stream, "PHY_EXT_MASK(30) = 0x%04X\n", irq_mask);
    printf (stream, "PHY_EXT_STAT(31) = 0x%04X\n", ext_state);
}

void eth_waiting_autonegotiation (void)
{
   while ((eth_get_phyreg(ARM_ETH_PHY_ADDRESS, ARM_ETH_PHY_BASIC_STAT) & (1<<5)) == 0); 
}

void eth_restart_autonegotiation (eth_t *u)
{
    uint16_t tmp;
    //mutex_lock(&u->netif.lock);
    tmp = eth_get_phyreg(ARM_ETH_PHY_ADDRESS, ARM_ETH_PHY_BASIC_CTRL);
    eth_set_phyreg(ARM_ETH_PHY_ADDRESS, ARM_ETH_PHY_BASIC_CTRL, tmp | (1<<9));
    while ((eth_get_phyreg(ARM_ETH_PHY_ADDRESS, ARM_ETH_PHY_BASIC_STAT) & (1<<5)) == 0);
    //eth_get_speed(u, (uint8_t*)&tmp);
    //eth_controller_init(macaddr, /*ARM_ETH_PHY_FULL_AUTO*/ARM_ETH_PHY_100BASET_HD_AUTO);
    //mutex_unlock(&u->netif.lock);
//  debug_printf("AUTONEG RST\n");
}

uint16_t eth_get_carrier (eth_t *u)
{
    return (ARM_ETH->PHY_STAT & ARM_ETH_PHY_LED_CRS) == 0;
}

uint32_t eth_get_speed(eth_t *u, uint8_t *duplex)
{
    uint16_t extctl;

    mutex_lock (&u->netif.lock);
    extctl = eth_get_phyreg (ARM_ETH_PHY_ADDRESS, ARM_ETH_PHY_EXT_CTRL);
    mutex_unlock (&u->netif.lock);

    switch (extctl & EXT_CTRL_SPEED_MASK(7)) {
    case EXT_CTRL_SPEED_10_HD:    /* 10Base-T half duplex */
        if (duplex)
            *duplex = 0;
        u->netif.bps = 10 * 1000000;
        break;
    case EXT_CTRL_SPEED_100_HD:   /* 100Base-TX half duplex */
        if (duplex)
            *duplex = 0;
        u->netif.bps = 100 * 1000000;
        break;
    case EXT_CTRL_SPEED_10_FD:    /* 10Base-T full duplex */
        if (duplex)
            *duplex = 1;
        u->netif.bps = 10 * 1000000;
        break;
    case EXT_CTRL_SPEED_100_FD:   /* 100Base-TX full duplex */
        if (duplex)
            *duplex = 1;
        u->netif.bps = 100 * 1000000;
        break;
    default:
        return 0;
    }
    return u->netif.bps;
}

void eth_set_loop(eth_t *u, int on)
{
    uint16_t control;

    mutex_lock (&u->netif.lock);

    // Set PHY loop-back mode (hort circuit mode)
    control = eth_get_phyreg(ARM_ETH_PHY_ADDRESS, ARM_ETH_PHY_BASIC_CTRL);
    if(on) {
        control |= EXT_CTRL_LPBK;
    } else {
        control &= ~EXT_CTRL_LPBK;
    }
    eth_set_phyreg(ARM_ETH_PHY_ADDRESS, ARM_ETH_PHY_BASIC_CTRL, control);
    
    // Set MAC loop-back mode (hort circuit mode)
    // сброс приемника и передатчика RRST=1, XRST=1 
    /*ARM_ETH->G_CFG_HI |= ARM_ETH_XRST | ARM_ETH_RRST;
    if(on)  {ARM_ETH->G_CFG_HI |= ARM_ETH_DLB;}
    else    {ARM_ETH->G_CFG_HI &= ~ARM_ETH_DLB;}
    ARM_ETH->G_CFG_HI &= ~(ARM_ETH_XRST | ARM_ETH_RRST);    //RRST=0, XRST=0 штатный режим работы
    asm("nop");*/
    
    mutex_unlock (&u->netif.lock);
}

void eth_set_promisc (eth_t *u, uint8_t station, uint8_t group)
{
    uint16_t rx_frame_control;
    mutex_lock (&u->netif.lock);
    
    // сброс приемника и передатчика RRST=1, XRST=1 
    ARM_ETH->G_CFG_HI |= ARM_ETH_XRST | ARM_ETH_RRST;
    
    rx_frame_control = ARM_ETH->R_CFG;
    
    // очистка флагов разрешения приёма
    rx_frame_control &= ~(ARM_ETH_AC_EN | ARM_ETH_MCA_EN | ARM_ETH_UCA_EN | ARM_ETH_BCA_EN);
    if (station) {
        // Accept any unicast.
        rx_frame_control |= ARM_ETH_UCA_EN |  // приём пакетов с MAC-адресом, указанным в регистреMAC_Address
                            ARM_ETH_BCA_EN;   // Прием пакетов с широковещательным MAC-адресом.
    } else if (group) {
        /* Accept any multicast. */       
        rx_frame_control |= ARM_ETH_MCA_EN | // Прием пакетов с групповым MAC-адресом с фильтрацией по HASH-таблице.
                            ARM_ETH_BCA_EN;  // Прием пакетов с широковещательным MAC-адресом.
    }
    ARM_ETH->R_CFG = rx_frame_control;
    ARM_ETH->G_CFG_HI &= ~(ARM_ETH_XRST | ARM_ETH_RRST);    //RRST=0, XRST=0 штатный режим работы
    eth_nops();
    mutex_unlock (&u->netif.lock);
}

// Poll for interrupts.
// Must be called by user in case there is a chance to lose an interrupt.
void eth_poll(eth_t *u)
{
    mutex_lock(&u->netif.lock);
    if (eth_handle_receive_interrupt(u))
        mutex_signal(&u->netif.lock, 0);
        //mutex_signal (&eth_interrupt_mutex, 0);
    mutex_unlock(&u->netif.lock);

    mutex_lock(&u->netif.lock);
    eth_handle_transmit_interrupt(u);
    mutex_unlock(&u->netif.lock);
}

void inline __attribute__((always_inline)) eth_nops(void)
{  
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
    asm volatile("nop");
}

void inline __attribute__((always_inline)) eth_err_0022_correction(void)
{ 
 // Ошибка 0022 шины AHB Ethernet контроллера, см errata, применено для случая когда
 // eth-буферы и/или pool-памяти расположены в data_hi
    asm volatile("dsb");
    //eth_nops ();
}

void eth_clk_init (void)
{   
    uint32_t temp;
    
    //Настройка стабилизатора 1.8 В на дополнительную нагруку (Частота ядра выше 80 МГц)
    ARM_RSTCLK->PER_CLOCK |= ARM_PER_CLOCK_BKP;             //BKP Clock enable
    temp = ARM_BACKUP->BKP_REG_0E;
    temp &= 0xFFFFFFC0;	
    ARM_BACKUP->BKP_REG_0E = temp | ARM_BKP_REG_0E_LOW | ARM_BKP_REG_0E_SELECT_RI;	// SelectRI = 0x7, LOW = 0x7; (for core frequency more then 80 MHz);

    //Настройка HSE2
#ifdef ARM_EXT_GEN2
    ARM_RSTCLK->HS_CONTROL |= ARM_HS_CONTROL_HSE2_BYP;  // Generator mode
    ARM_RSTCLK->HS_CONTROL |= ARM_HS_CONTROL_HSE2_ON;
#else
    ARM_RSTCLK->HS_CONTROL |= ARM_HS_CONTROL_HSE2_ON;   // Oscillator mode;
#endif
    while((ARM_RSTCLK->CLOCK_STATUS & ARM_CLOCK_STATUS_HSE_RDY2) == 0);

    temp = ARM_RSTCLK->ETH_CLOCK;
    temp &= ~(ARM_ETH_CLOCK_ETH_BRG(0xFF) | ARM_ETH_CLOCK_PHY_SEL(ARM_ETH_CLOCK_PHY_SEL_HSE2) | ARM_ETH_CLOCK_PHY_BRG(0xFF));

    //Соотв. источник тактирования HSE2, вкл тактирования физики, вкл тактирование MAC
    temp |= ARM_ETH_CLOCK_ETH_BRG(0) | ARM_ETH_CLOCK_PHY_SEL(ARM_ETH_CLOCK_PHY_SEL_HSE2) | ARM_ETH_CLOCK_PHY_BRG(0) | ARM_ETH_CLOCK_PHY_EN | ARM_ETH_CLOCK_ETH_EN;
    ARM_RSTCLK->ETH_CLOCK = temp;
    eth_nops();
}

// инициализация модуля PHY новыми значениями адреса и режима работы
void eth_phy_init (uint8_t addr, uint8_t mode)
{
    // assertion
    if(addr > 0x1F) {
        addr = 0x1F;
        debug_printf ("error parametr addr in eth_phy_init()\n");
    }
    if(mode > 0x07) {
        mode = 0x07;
        debug_printf ("error parametr mode in eth_phy_init()\n");
    }
    ARM_ETH->PHY_CTRL = 0;
    volatile unsigned i;
    //ARM_GPIOD->SETTX = (1 << 14);
    for (i=0;i<KHZ*10/5;i++); // 24 мс
    //ARM_GPIOD->CLRTX = (1 << 14);
    ARM_ETH->PHY_CTRL = ARM_ETH_PHY_ADDR(addr) | ARM_ETH_PHY_MODE(mode) | ARM_ETH_PHY_NRST;
    for (i=0;i<KHZ*10/6;i++); // Ожидание 20 мс (по даташиту 16 мс)
    while((ARM_ETH->PHY_STAT & ARM_ETH_PHY_READY) == 0);    //ждем пока модуль в состоянии сброса
}

void eth_mac_init (uint8_t PHYMode)
{
    // сброс приемника и передатчика RRST=1, XRST=1 автоматическое изменение указателей запрещено
    ARM_ETH->IMR = 0;
    ARM_ETH->IFR = 0;
    
    ARM_ETH->G_CFG_HI = ARM_ETH_XRST | ARM_ETH_RRST;                // Включение состояния сброса приёмника и передатчика
    //memset((uint8_t*)ARM_ETH_BUF_BASE, 0, ARM_ETH_BUF_FULL_SIZE);
    
#ifdef ETH_BUFTYPE_FIFO
    ARM_ETH->G_CFG_HI |=  ARM_ETH_DBG_XF_EN | ARM_ETH_DBG_RF_EN;    // разрешить автоматическую установку указателей приёмника и передатчика - для FIFO
    ARM_ETH->G_CFG_LOW = ARM_ETH_BUFF_MODE(ARM_ETH_BUFF_FIFO) |     // буфферы в режиме FIFO
#else
    ARM_ETH->G_CFG_LOW = ARM_ETH_BUFF_MODE(ARM_ETH_BUFF_LINEAL) |   // буфферы в линейном режиме
#endif
    ARM_ETH_PAUSE_EN|                                               // режим автоматической обработки пакета PAUSE
    ARM_ETH_COLWND(128);                                            // размер окна коллизий (в битовых интервалах)
                                                                    // сброс флагов IRF производится записью в IRF
#ifdef ETH_DTRM
    ARM_ETH->G_CFG_LOW |= ARM_ETH_DTRM_EN;                          // режим определенного времени доставки
#endif
    ARM_ETH->DELIMITER = ARM_ETH_BUF_SIZE_R; // регистр границы буферов приемника и передатчика (первый байт буффера передатчика)
    ARM_ETH->R_HEAD = 0;                     // указывает на первое непустое слово в буфере приемника
    ARM_ETH->X_TAIL = ARM_ETH_BUF_SIZE_R;    // указывает на первое пустое слово в буффере передатчика, зависит от размера буффера приёмника

    ARM_ETH->HASH[0] = 0;    ARM_ETH->HASH[1] = 0;
    ARM_ETH->HASH[2] = 0;    ARM_ETH->HASH[3] = 0;
    ARM_ETH->HASH[4] = 0;    ARM_ETH->HASH[5] = 0;
    ARM_ETH->HASH[6] = 0x80; ARM_ETH->HASH[7] = 0;
    
    // Настройки формирования очереди пакетов (влияют при включенном ARM_ETH_DTRM_EN)
    // межпакетный интервал для полнодуплексного режима
    ARM_ETH->IPG = 96;  // (для 100 Мбит - 16);
    // предделитель BAG и JitterWnd
    ARM_ETH->PSC = 50;  // (для 100 Мбит - 10); 
    // период следования пакетов
    ARM_ETH->BAG = 100; // (для 100 Мбит - 50); 
    // джиттер при передачи пакетов
    ARM_ETH->JITTER_WND = 4;

    // управление приемником 
    ARM_ETH->R_CFG =
    ARM_ETH_EN |                                        /* разрешение работы приемника */
    ARM_ETH_EVNT_MODE(ARM_ETH_EVNT_RX_DONE)|            /* событие -  прием пакета завершен */
    ARM_ETH_CF_EN|                                      /* разрешение приема управляющих пакетов */
    ARM_ETH_UCA_EN |                                    /* прием пакетов с MAC указанном в регистре MAC_Address */
    ARM_ETH_BCA_EN ;                                    /* прием широковещательных MAC */
    /*ARM_ETH_AC_EN|;*/                                 /* прием без фильтрации */
    /*ARM_ETH_MCA_EN;*/                                 /* прием c фильтрацией по HASN таблице */

    // управление передатчиком
    ARM_ETH->X_CFG =
    ARM_ETH_EN |                                        /* разрешение работы передатчика */
    ARM_ETH_EVNT_MODE(ARM_ETH_EVNT_TX_DONE)|            /* событие - отправка пакета завершена */
    ARM_ETH_PAD_EN |                                    /* разрешено дополнение PAD'ми */
    ARM_ETH_PRE_EN |                                    /* дополнение преамбулой */
    ARM_ETH_CRC_EN |                                    /* дополнение CRC */
    ARM_ETH_IPG_EN |                                    /* выдержка паузы */
    ARM_ETH_RTRYCNT(0x0F);                              /* количество попыток обмена */

    ARM_ETH->G_CFG_HI &= ~(ARM_ETH_XRST | ARM_ETH_RRST);//RRST=0, XRST=0 штатный режим работы
}

void eth_restart_receiver(void)
{
    ARM_ETH->G_CFG_HI |= ARM_ETH_RRST;
    memset((uint8_t*)ARM_ETH_BUF_BASE_R, 0, ARM_ETH_BUF_SIZE_R);
    ARM_ETH->R_HEAD = 0;
    ARM_ETH->STAT &= ~ARM_ETH_R_COUNT(7);
    ARM_ETH->IFR |= 0x00FF;
    eth_nops();
    ARM_ETH->G_CFG_HI &= ~ ARM_ETH_RRST;
    eth_err_0022_correction();
}

void eth_rx_buffer_clr(void)
{
 // альтернатива eth_restart_receiver
    ARM_ETH->R_HEAD = ARM_ETH->R_TAIL; 
    memset((uint8_t*)ARM_ETH_BUF_BASE_R, 0, ARM_ETH_BUF_SIZE_R);
    ARM_ETH->STAT &= ~ARM_ETH_R_COUNT(7);
    eth_err_0022_correction();
}

void eth_restart_transmitter(void)
{
    ARM_ETH->X_TAIL = ARM_ETH->X_HEAD;
    ARM_ETH->G_CFG_HI |= ARM_ETH_XRST;
    eth_nops();
    ARM_ETH->G_CFG_HI &= ~ARM_ETH_XRST;	
    eth_err_0022_correction();
}

void eth_tx_buffer_clr(void)
{
    ARM_ETH->X_HEAD = ARM_ETH->X_TAIL; 
    memset((uint8_t*)ARM_ETH_BUF_BASE_X, 0, ARM_ETH_BUF_SIZE_X);
    eth_err_0022_correction();
}

uint16_t calc_R_filled_space(void)
{
    uint16_t data_start = ARM_ETH->R_HEAD;
    uint16_t data_end = ARM_ETH->R_TAIL; 
    uint16_t data_space=0;	
    
    if(data_start < data_end) { 
        data_space = data_end - data_start;
    } else if(data_start > data_end) {
        data_space = ARM_ETH_BUF_SIZE_R - data_start + data_end; // ситуация когда буффер "закольцован". вычисляется кол-во байтов до ARM_ETH_BUF_SIZE_R
    }
    return data_space;
}

void eth_pin_init (GPIO_t *gpio, unsigned port, unsigned pin, unsigned func, unsigned in)
{
    // Подача синхроимпульсов
    ARM_RSTCLK->PER_CLOCK |= (ARM_PER_CLOCK_GPIOA << port);
    // Установка функции 
    gpio->FUNC = (gpio->FUNC & ~ARM_FUNC_MASK(pin)) | ARM_FUNC(pin, func);
    // Цифровой вывод
    gpio->ANALOG |= (1 << pin);
    // Быстрый фронт 
    gpio->PWR = (gpio->PWR & ~ARM_PWR_MASK(pin)) | ARM_PWR_FASTEST(pin);
    // Выход
    if(in) 
        gpio->OE |= ARM_GPIO_OUT(pin);
}

void eth_port_init (void)
{
    ARM_RSTCLK->PER_CLOCK |= ARM_PER_CLOCK_GPIOF;
    // ARM_GPIOF->ANALOG &= ~(ARM_ANALOG_MASK(14) | ARM_ANALOG_MASK(15)); //Подключено к модулю тактирования ethernet по умолчанию
}

void eth_led_init (void)
{
    //  EVAL_BOARD PB14 - жёлттый СИД на ethernet разъёме
    // 	EVAL_BOARD PB15 - зелёный СИД на ethernet разъёме
    eth_pin_init (ETH_CRS_GPIO, PORT(ETH_CRS_LED), PIN(ETH_CRS_LED), FUNC_PORT, 1);
    eth_pin_init (ETH_LINK_GPIO, PORT(ETH_LINK_LED), PIN(ETH_LINK_LED), FUNC_PORT, 1);
    eth_pin_init (ETH_SPEED_GPIO, PORT(ETH_SPEED_LED), PIN(ETH_SPEED_LED), FUNC_PORT, 1);
    eth_pin_init (ETH_HD_GPIO, PORT(ETH_HD_LED), PIN(ETH_HD_LED), FUNC_PORT, 1);
}

#ifdef ETH_USE_DMA
void eth_dma_init (void)
{
    // Для режима АВТО-ЗАПРОС
    ARM_RSTCLK->PER_CLOCK |= ARM_PER_CLOCK_DMA;   
    ARM_DMA->CHNL_REQ_MASK_SET = ARM_DMA_DISABLE_ALL;   // disable all requests
    ARM_DMA->CHNL_ENABLE_CLR = ARM_DMA_DISABLE_ALL;     // disable all channels
    //ARM_DMA->CHNL_ENABLE_SET = 0xFFFFFFFF;
    ARM_DMA->CHNL_PRI_ALT_CLR= ARM_DMA_DISABLE_ALL;     // !! all channel use primary management structure
    ARM_DMA->ERR_CLR = ARM_DMA_ERR_CLR;
    ARM_DMA->CTRL_BASE_PTR = (unsigned) dma_conf;
    ARM_DMA->CONFIG |= ARM_DMA_ENABLE;
    dma_conf[ETH_DMA_CHN_TX].UNUSED = 0;
    dma_conf[ETH_DMA_CHN_RX].UNUSED = 0;
}
#endif

// Set values to Ethernet controller registers.
void eth_controller_init (const uint8_t *MACAddr, uint8_t PHYMode)
{
    // отключение прерываний
    ARM_ETH->IMR=0;
    ARM_ETH->IFR=0xFFFF;
    eth_port_init();
    eth_clk_init();
    eth_phy_init(ARM_ETH_PHY_ADDRESS, PHYMode); //PHY address 0x1C
    eth_mac_init(PHYMode);
    memcpy((void*)ARM_ETH->MAC_ADDR, MACAddr, 6);
 #ifdef ETH_USE_DMA
    eth_dma_init();
 #endif
}

// Функция для записи регистров PHY модуля через MDIO интерфейс 
// addr	- адрес модуля PHY
// num	- номер регистра PHY
// data	- данные для записи
void eth_set_phyreg (uint8_t addr, uint8_t num, uint16_t data)
{
    ARM_ETH->MDIO_DATA = data;
    ARM_ETH->MDIO_CTRL = ARM_ETH_MDIO_RDY | ARM_ETH_MDIO_PRE_EN | ARM_ETH_MDIO_PHY_A(addr & 0x1F) |
                         ARM_ETH_MDIO_DIV(1) | ARM_ETH_MDIO_RG_A(num & 0x1F);
    while((ARM_ETH->MDIO_CTRL & ARM_ETH_MDIO_RDY)==0);
}

// Функция для чтения регистров PHY модуля через MDIO интерфейс 
// addr - адрес модуля PHY
// num  - номер регистра, который необходимо прочитать
// возвращает значение регистра по адресу Num в Addr модуле PHY.
uint16_t eth_get_phyreg (uint8_t addr, uint8_t num)
{
    ARM_ETH->MDIO_CTRL = ARM_ETH_MDIO_RDY | ARM_ETH_MDIO_PRE_EN | ARM_ETH_MDIO_OP | ARM_ETH_MDIO_PHY_A(addr & 0x1F) |
                         ARM_ETH_MDIO_DIV(1) | ARM_ETH_MDIO_RG_A(num & 0x1F);
    while((ARM_ETH->MDIO_CTRL & ARM_ETH_MDIO_RDY)==0); 
    return	ARM_ETH->MDIO_DATA;
}

void eth_send_processing (const void *buf, uint16_t frame_length)
{
 #ifdef ETH_USE_DMA
    eth_send_frame_dma (buf, frame_length);
 #else
    eth_send_frame (buf, frame_length);
 #endif
}

uint32_t eth_read_processing (void *buf)
{
 #ifdef ETH_USE_DMA
    return eth_read_frame_dma (buf);
 #else
    return eth_read_frame (buf);
 #endif
}

// чтение статуса из буффера приемника
uint32_t eth_rx_status (void)
{
    // адрес поля статуса приёма пакета находится перед началом данных
    return *(uint32_t*)(ARM_ETH_BUF_BASE + ARM_ETH->R_HEAD);
}

#ifdef ETH_USE_DMA
// запись кадра в буффер передатчика. возвращает указатель на область куда будет записано пеле статуса
volatile int waitDma, cyclFrame;
void eth_send_frame_dma (const void *buf, uint16_t length_bytes) // size в байтах, buf мб 8-разрядным 
{
#ifdef ETH_BUFTYPE_FIFO
    uint32_t source = (uint32_t)buf;
    uint32_t length_words = (length_bytes+3)>>2;
    length_bytes = length_words << 2;

    ARM_ETH_TX_FIFO = length_bytes;
    dma_conf[ETH_DMA_CHN_TX].SOURCE_END_POINTER = source + length_bytes-4;
    //debug_printf ("DMA SOURCE_END_POINTER = 0x%08X\n",dma_conf[ARM_DMA_CHN_1].SOURCE_END_POINTER);
    dma_conf[ETH_DMA_CHN_TX].DEST_END_POINTER = ARM_ETH_BUF_BASE + 0x04;
    dma_conf[ETH_DMA_CHN_TX].CONTROL = ARM_DMA_DST_INC(ARM_DMA_ADDR_NOINC) | ARM_DMA_DST_SIZE(ARM_DMA_WORD) |
                                       ARM_DMA_SRC_INC(ARM_DMA_WORD) | ARM_DMA_SRC_SIZE(ARM_DMA_WORD) |
                                       ARM_DMA_RPOWER(15) | ARM_DMA_TRANSFERS(length_words) | ARM_DMA_AUTOREQ;
    ARM_DMA->CHNL_PRIORITY_SET |= ARM_DMA_SELECT(ETH_DMA_CHN_TX);
    ARM_DMA->CHNL_USEBURST_CLR = ARM_DMA_SELECT(ETH_DMA_CHN_TX);
    ARM_DMA->CHNL_REQ_MASK_CLR = ARM_DMA_SELECT(ETH_DMA_CHN_TX);
    ARM_DMA->CHNL_ENABLE_SET = ARM_DMA_SELECT(ETH_DMA_CHN_TX);
    waitDma = 1;
    ARM_DMA->CHNL_SW_REQUEST = ARM_DMA_SELECT(ETH_DMA_CHN_TX);
    
    //while ((dma_conf[ETH_DMA_CHN_TX].CONTROL & (ARM_DMA_TRANSFERS(1024) | ARM_DMA_AUTOREQ)) != 0) {
    //    eth_err_0022_correction();
    //}
    while (waitDma) { eth_err_0022_correction(); /*task_yield (); */}
    //mutex_wait (&dma_mutex);
    
    //ARM_DMA->CHNL_ENABLE_CLR = ARM_DMA_SELECT(ETH_DMA_CHN_TX);
    
    ARM_ETH_TX_FIFO = 0; // Без этой записи передача не работает
    ARM_ETH_TX_FIFO = 0; // Без этой записи обмен "падает"
    eth_err_0022_correction();
#else
    uint16_t data_space[2];
    uint16_t length_words;
    uint16_t data_start, data_end;  // адрес начала, области данных и первого пустого слова в буффере передатчика
    uint32_t source, dest;
    //uint32_t tx_status;           // статусы, устанавлюваются уже после попытки передачи пакета
//    debug_printf ("ETH_TX_IN\n");
    data_start = ARM_ETH->X_HEAD;
    data_end = ARM_ETH->X_TAIL;

    // количество свободных байт в буфере передатчика
    if(data_start > data_end) {
        // данные "закольцованы" и адрес начала данных больше адреса конца данных
        data_space[0] = data_start - data_end;
        data_space[1] = 0;
    } else {
        data_space[0] = ARM_ETH_BUF_FULL_SIZE - data_end;
        data_space[1] = data_start - (uint16_t)ARM_ETH_BUF_BASE_X;
    }
    length_words = (length_bytes+3)>>2;	// длина с в словах учётом выравнивания

    // если места в буфере меньше, чем кол-во байт данных (+2 - учет 2х 32-разрядных информационных полей см даташит)
    //if(((data_space[0]+data_space[1])>>2) >= (length_words +2)) {
        source = (uint32_t)buf;
        dest = (uint32_t)(ARM_ETH_BUF_BASE + data_end); // адрес в буфере для записи нового пакета данных
    
        *(uint32_t*)dest = length_bytes; // заполнение поля управления передачи пакета (кол-во байт в пакете)
        eth_err_0022_correction();
        dest += 4;
        data_space[0] -= 4;     // соотв. свободное место буффера уменьшилось на 32-разрядное слово.

        if(dest >= (uint32_t)(ARM_ETH_BUF_BASE + ARM_ETH_BUF_FULL_SIZE)) { // "закольцовка" 
            dest = (uint32_t)ARM_ETH_BUF_BASE_X;
        }
        
        length_bytes = length_words << 2;
        if(length_bytes <= data_space[0]) {             
            // кадр не надо "закольцовывать"
            dma_conf[ETH_DMA_CHN_TX].SOURCE_END_POINTER = source + length_bytes-4;
            //debug_printf ("DMA SOURCE_END_POINTER = 0x%08X\n",dma_conf[ARM_DMA_CHN_1].SOURCE_END_POINTER);
            dma_conf[ETH_DMA_CHN_TX].DEST_END_POINTER = dest + length_bytes-4;
            dma_conf[ETH_DMA_CHN_TX].CONTROL = ARM_DMA_DST_INC(ARM_DMA_WORD) | ARM_DMA_DST_SIZE(ARM_DMA_WORD) |
                                            ARM_DMA_SRC_INC(ARM_DMA_WORD) | ARM_DMA_SRC_SIZE(ARM_DMA_WORD) |
                                            /*ARM_DMA_RPOWER(1) |*/ ARM_DMA_TRANSFERS(length_words) | ARM_DMA_AUTOREQ;
            ARM_DMA->CHNL_REQ_MASK_CLR = ARM_DMA_SELECT(ETH_DMA_CHN_TX);
            ARM_DMA->CHNL_USEBURST_CLR = ARM_DMA_SELECT(ETH_DMA_CHN_TX);
            ARM_DMA->CHNL_ENABLE_SET = ARM_DMA_SELECT(ETH_DMA_CHN_TX);
            cyclFrame = 0;
            waitDma = 1; 
//            debug_printf ("noCyclFrame %d\n", length_words);
            ARM_DMA->CHNL_SW_REQUEST = ARM_DMA_SELECT(ETH_DMA_CHN_TX);
        } else { 
            // кадр закольцован
            dma_conf[ETH_DMA_CHN_TX].SOURCE_END_POINTER = source + data_space[0]-4;
            dma_conf[ETH_DMA_CHN_TX].DEST_END_POINTER = dest + data_space[0]-4;
                    
            source += data_space[0];
            data_space[0] >>= 2; // делим на 4 (кол-во 32-разрядных слов)
            
            dma_conf[ETH_DMA_CHN_TX].CONTROL = ARM_DMA_DST_INC(ARM_DMA_WORD) | ARM_DMA_DST_SIZE(ARM_DMA_WORD) |
                                            ARM_DMA_SRC_INC(ARM_DMA_WORD) | ARM_DMA_SRC_SIZE(ARM_DMA_WORD) |
                                            /*ARM_DMA_RPOWER(1) |*/ ARM_DMA_TRANSFERS(data_space[0]) | ARM_DMA_AUTOREQ;
            ARM_DMA->CHNL_REQ_MASK_CLR = ARM_DMA_SELECT(ETH_DMA_CHN_TX);
            ARM_DMA->CHNL_USEBURST_CLR = ARM_DMA_SELECT(ETH_DMA_CHN_TX);
            ARM_DMA->CHNL_ENABLE_SET = ARM_DMA_SELECT(ETH_DMA_CHN_TX);
            cyclFrame = 0;
            waitDma = 1;
//            debug_printf ("cyclFrame 1 %d\n", length_words);
            ARM_DMA->CHNL_SW_REQUEST = ARM_DMA_SELECT(ETH_DMA_CHN_TX);
            
            //while ((dma_conf[ETH_DMA_CHN_TX].CONTROL & (ARM_DMA_TRANSFERS(1024) | ARM_DMA_AUTOREQ)) != 0) {
            //    eth_err_0022_correction();
            //}
            while (waitDma) { eth_err_0022_correction(); /*task_yield (); */}
            //mutex_wait (&dma_mutex);
            
            // считывание области 0....(frame_length)
            dest = (uint32_t)ARM_ETH_BUF_BASE_X; 
            length_words -= data_space[0];
            length_bytes = length_words << 2;
            dma_conf[ETH_DMA_CHN_TX].SOURCE_END_POINTER = source + length_bytes-4;
            dma_conf[ETH_DMA_CHN_TX].DEST_END_POINTER = dest + length_bytes - 4;
            dma_conf[ETH_DMA_CHN_TX].CONTROL = ARM_DMA_DST_INC(ARM_DMA_WORD) | ARM_DMA_DST_SIZE(ARM_DMA_WORD) |
                                               ARM_DMA_SRC_INC(ARM_DMA_WORD) | ARM_DMA_SRC_SIZE(ARM_DMA_WORD) |
                                               /*ARM_DMA_RPOWER(1) |*/ ARM_DMA_TRANSFERS(length_words) | ARM_DMA_AUTOREQ;
            ARM_DMA->CHNL_REQ_MASK_CLR = ARM_DMA_SELECT(ETH_DMA_CHN_TX);
            ARM_DMA->CHNL_USEBURST_CLR = ARM_DMA_SELECT(ETH_DMA_CHN_TX);
            ARM_DMA->CHNL_ENABLE_SET = ARM_DMA_SELECT(ETH_DMA_CHN_TX);
            cyclFrame = 0;
            waitDma = 1;
//            debug_printf ("cyclFrame 2 %d\n", length_words);
            ARM_DMA->CHNL_SW_REQUEST = ARM_DMA_SELECT(ETH_DMA_CHN_TX);
            
            // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            // todo В некоторых случаях обмен может падать без этой вставки
            //if((dest+length_bytes) == (uint32_t)(ARM_ETH_BUF_BASE + ARM_ETH_BUF_FULL_SIZE)) 
            //    length_bytes -= 4;
            // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        }
        //while ((dma_conf[ETH_DMA_CHN_TX].CONTROL & (ARM_DMA_TRANSFERS(1024) | ARM_DMA_AUTOREQ)) != 0) {
        //    eth_err_0022_correction();
        //}
        while (waitDma) { eth_err_0022_correction(); /*task_yield (); */}
        //mutex_wait (&dma_mutex);
        
        //ARM_DMA->CHNL_ENABLE_CLR = ARM_DMA_SELECT(ETH_DMA_CHN_TX);
        
        dest+=length_bytes;
        if(dest >= (uint32_t)(ARM_ETH_BUF_BASE + ARM_ETH_BUF_FULL_SIZE)) { // "закольцовка буффера"
            dest = (uint32_t)ARM_ETH_BUF_BASE_X;
        }
        //tx_status = dest;
        *(uint32_t*)dest = 0;    // обнуление слова за данными, в это слово записывается поле состояния передачи пакета(см даташит)
        dest += 4;

        if(dest >= (uint32_t)(ARM_ETH_BUF_BASE + ARM_ETH_BUF_FULL_SIZE)) { // "закольцовка буффера"
            dest = (uint32_t)ARM_ETH_BUF_BASE_X;
        }
        ARM_ETH->X_TAIL = (uint16_t)dest; 
        eth_err_0022_correction();  
//        debug_printf ("ETH_TX_OUT\n");
    //}
    // можно сохранять адрес tx_status, чтобы после отсыла пакета считать статус.
#endif
}

// чтение кадра из буффера приемника, для режима АВТО-ЗАПРОС
uint32_t eth_read_frame_dma (void *buf)
{
#ifdef ETH_BUFTYPE_FIFO
    uint32_t dest = (uint32_t)buf;
    uint32_t rx_status = ARM_ETH_RX_FIFO;
    uint32_t length_bytes = (ARM_ETH_PKT_LENGTH(rx_status) > ETH_MTU) ? ETH_MTU : ARM_ETH_PKT_LENGTH(rx_status);
    uint32_t length_words = (length_bytes+3)>>2;
    length_bytes = length_words << 2;

    dma_conf[ETH_DMA_CHN_RX].SOURCE_END_POINTER = ARM_ETH_BUF_BASE;
    dma_conf[ETH_DMA_CHN_RX].DEST_END_POINTER = dest + length_bytes-4;
    dma_conf[ETH_DMA_CHN_RX].CONTROL = ARM_DMA_DST_INC(ARM_DMA_WORD) | ARM_DMA_DST_SIZE(ARM_DMA_WORD) |
                                       ARM_DMA_SRC_INC(ARM_DMA_ADDR_NOINC) | ARM_DMA_SRC_SIZE(ARM_DMA_WORD) |
                                       ARM_DMA_RPOWER(1) | ARM_DMA_TRANSFERS(length_words) | ARM_DMA_AUTOREQ;
    ARM_DMA->CHNL_PRIORITY_SET |= ARM_DMA_SELECT(ETH_DMA_CHN_RX);
    ARM_DMA->CHNL_USEBURST_CLR = ARM_DMA_SELECT(ETH_DMA_CHN_RX);
    ARM_DMA->CHNL_REQ_MASK_CLR = ARM_DMA_SELECT(ETH_DMA_CHN_RX);
    ARM_DMA->CHNL_ENABLE_SET = ARM_DMA_SELECT(ETH_DMA_CHN_RX);
    ARM_DMA->CHNL_SW_REQUEST = ARM_DMA_SELECT(ETH_DMA_CHN_RX);
   
    while ((dma_conf[ETH_DMA_CHN_RX].CONTROL & (ARM_DMA_TRANSFERS(1024) | ARM_DMA_AUTOREQ)) != 0);
    //ARM_DMA->CHNL_ENABLE_CLR = ARM_DMA_SELECT(ETH_DMA_CHN_RX);

    if(ARM_ETH->STAT & ARM_ETH_R_COUNT(7)) {
        ARM_ETH->STAT -= ARM_ETH_R_COUNT(1);    // минус один кадр
    }
    eth_err_0022_correction(); 

    return rx_status;
#else
    uint16_t data_start, data_end;  // start - адрес первого байта данных / end - адрес байта, следующего за последним байтом данных
    uint16_t data_space;
    uint16_t length_words;          // кол-во несчитанных байт кадра в словах
    uint16_t length_bytes;          // кол-во несчитанных байт кадра в байтах
    uint32_t source;                // источник данных
    uint32_t dest;                  // ethernet-буфер
    uint32_t rx_status=0;             // поле состояния приема пакета

    // если нет принятых (еще не обработанных пакетов)
    if(ARM_ETH->R_HEAD != ARM_ETH->R_TAIL) {
        data_start = ARM_ETH->R_HEAD;
        data_end   = ARM_ETH->R_TAIL;
        // R_TAIL меняется автоматически, в зависимости от длины данных,
        // указывает на пустую ячейку, следующую за данными(которые могут содержать несколько кадров).

        // количество байт данных в пакете, расчитанно из адреса начала данных и начала пустой области
        if(data_start < data_end) {
            data_space = data_end - data_start; // общее кол-во байтов (может включать несколько кадров) 
        } else {
            data_space = ARM_ETH_BUF_SIZE_R - data_start; // ситуация когда буффер "закольцован". вычисляется кол-во байтов до ARM_ETH_BUF_SIZE_R
        }
        source = (uint32_t)(ARM_ETH_BUF_BASE + data_start);
        dest = (uint32_t)buf;
        rx_status = *(uint32_t*)source;  // поле состояния приема кадра (количество байт данных в кадре, включая заголовок и FCS и статус)
        source += 4;
        length_bytes = (uint16_t)rx_status;
        
        // размер data_space может быть боольше чем frame_length из-за выравнивания по слову.

        data_start += 4;  // установка адреса на начало ещё не прочитанных данных.
        data_space -= 4;  // отнимаются 4 считанных байта поля состояния приема пакета. остаётся "чистый" кадр;
        
        //debug_printf ("\n(uint32_t)source & 0x0000FFFF) = %08X\n",(uint32_t)source & 0x0000FFFF);

        if((source & 0x0000FFFF) > (ARM_ETH_BUF_SIZE_R-1)) { // закольцовка буффера
            source = (uint32_t)ARM_ETH_BUF_BASE;
        }
        length_words = (length_bytes+3)>>2;     // длина с в словах учётом выравнивания.
        length_bytes = length_words << 2; 

        eth_err_0022_correction();  // Если до вызова этой ф-ии была запись в eth-регистры
        if(length_bytes <= data_space) {
            // считывание области R_HEAD....конец кадра
            dma_conf[ETH_DMA_CHN_RX].SOURCE_END_POINTER = source + length_bytes-4;
            dma_conf[ETH_DMA_CHN_RX].DEST_END_POINTER = dest + length_bytes-4;
            dma_conf[ETH_DMA_CHN_RX].CONTROL = ARM_DMA_DST_INC(ARM_DMA_WORD) | ARM_DMA_DST_SIZE(ARM_DMA_WORD) |
                                               ARM_DMA_SRC_INC(ARM_DMA_WORD) | ARM_DMA_SRC_SIZE(ARM_DMA_WORD) |
                                               ARM_DMA_RPOWER(2) | ARM_DMA_TRANSFERS(length_words) | ARM_DMA_AUTOREQ;                       
            ARM_DMA->CHNL_REQ_MASK_CLR = ARM_DMA_SELECT(ETH_DMA_CHN_RX);
            ARM_DMA->CHNL_ENABLE_SET = ARM_DMA_SELECT(ETH_DMA_CHN_RX);
            ARM_DMA->CHNL_SW_REQUEST = ARM_DMA_SELECT(ETH_DMA_CHN_RX);
        } else {
            // требуются два подхода в считывании (условие, когда кадр "закольцован" в буффере)
            // считывание области R_HEAD....ARM_ETH_BUF_SIZE_R
            dma_conf[ETH_DMA_CHN_RX].SOURCE_END_POINTER = source + data_space-4;
            dma_conf[ETH_DMA_CHN_RX].DEST_END_POINTER = dest + data_space-4;

            dest += data_space;
            data_space >>= 2;

            dma_conf[ETH_DMA_CHN_RX].CONTROL = ARM_DMA_DST_INC(ARM_DMA_WORD) | ARM_DMA_DST_SIZE(ARM_DMA_WORD) |
                                               ARM_DMA_SRC_INC(ARM_DMA_WORD) | ARM_DMA_SRC_SIZE(ARM_DMA_WORD) |
                                               ARM_DMA_RPOWER(2) | ARM_DMA_TRANSFERS(data_space) | ARM_DMA_AUTOREQ;                       
            ARM_DMA->CHNL_REQ_MASK_CLR = ARM_DMA_SELECT(ETH_DMA_CHN_RX);
            ARM_DMA->CHNL_ENABLE_SET = ARM_DMA_SELECT(ETH_DMA_CHN_RX);
            ARM_DMA->CHNL_SW_REQUEST = ARM_DMA_SELECT(ETH_DMA_CHN_RX);

            while ((dma_conf[ETH_DMA_CHN_RX].CONTROL & (ARM_DMA_TRANSFERS(1024) | ARM_DMA_AUTOREQ)) != 0);

            // считывание области 0....(frame_length)
            source = (uint32_t)ARM_ETH_BUF_BASE;
            length_words -= data_space;
            length_bytes = length_words << 2;

            dma_conf[ETH_DMA_CHN_RX].SOURCE_END_POINTER = source + length_bytes-4;
            dma_conf[ETH_DMA_CHN_RX].DEST_END_POINTER = dest + length_bytes - 4;
            dma_conf[ETH_DMA_CHN_RX].CONTROL = ARM_DMA_DST_INC(ARM_DMA_WORD) | ARM_DMA_DST_SIZE(ARM_DMA_WORD) |
                                               ARM_DMA_SRC_INC(ARM_DMA_WORD) | ARM_DMA_SRC_SIZE(ARM_DMA_WORD) |
                                               ARM_DMA_RPOWER(2) | ARM_DMA_TRANSFERS(length_words) | ARM_DMA_AUTOREQ;                       
            ARM_DMA->CHNL_REQ_MASK_CLR = ARM_DMA_SELECT(ETH_DMA_CHN_RX);
            ARM_DMA->CHNL_ENABLE_SET = ARM_DMA_SELECT(ETH_DMA_CHN_RX);
            ARM_DMA->CHNL_SW_REQUEST = ARM_DMA_SELECT(ETH_DMA_CHN_RX);
        }
        while ((dma_conf[ETH_DMA_CHN_RX].CONTROL & (ARM_DMA_TRANSFERS(1024) | ARM_DMA_AUTOREQ)) != 0);
        //ARM_DMA->CHNL_ENABLE_CLR = ARM_DMA_SELECT(ETH_DMA_CHN_RX);
        
        source+=length_bytes;

        if((source & 0x0000FFFF) > ((uint32_t)(ARM_ETH_BUF_SIZE_R-1))) {
            source = (uint32_t)ARM_ETH_BUF_BASE;
        }
        ARM_ETH->R_HEAD = (uint16_t)source; // указание свободной для приёма области (первая пустая ячейка после данного пакета)
    }
    if(ARM_ETH->STAT & ARM_ETH_R_COUNT(7)) {
        ARM_ETH->STAT -= ARM_ETH_R_COUNT(1);    // минус один кадр
    }
    eth_err_0022_correction();
    return rx_status;
    #endif
}       
#else

// запись кадра в буффер передатчика. возвращает указатель на область куда будет записано пеле статуса
void eth_send_frame (const void *buf, uint16_t length_bytes) // size в байтах, buf мб 8-разрядным 
{
#ifdef ETH_BUFTYPE_FIFO
    uint32_t *source = (uint32_t*)buf;
    uint32_t length_words = (length_bytes+3)>>2;
    uint32_t i;
    
//    if((ARM_ETH->STAT & (ARM_ETH_X_FULL | ARM_ETH_X_AFULL)) == 0) { // буфер передатчика не в состояниях полон и почти полон 
        ARM_ETH_TX_FIFO = length_bytes;
        for(i=0; i<length_words; i++)
            ARM_ETH_TX_FIFO = *source++;
        ARM_ETH_TX_FIFO = 0; // Инкремент указателя для статусного слова
        
        // Ответ с форума ПК Миландр на запрос примера с FIFO
        // Petr: Примера нет, но надо записывать в FIFO и считывать из FIFO Ethernet на 1 слово больше чем необходимо отправить 
        // или указано в поле состояния приема пакета.
        // На практике запись дополнительного слова в FIFO передатчика помогает, однако для приёмника это вызывает ошибку.
        ARM_ETH_TX_FIFO = 0;
//    }
    eth_err_0022_correction();
#else
    uint16_t i;
    uint16_t data_space[2];
    uint16_t data_start, data_end;  // адрес начала, области данных и первого пустого слова в буффере передатчика
    uint32_t length_words;  
    uint32_t *source, *dest; 
    //uint32_t *tx_status;          // статусы, устанавлюваются уже после попытки передачи пакета

    data_start = ARM_ETH->X_HEAD;
    data_end = ARM_ETH->X_TAIL;

    // количество свободных байт в буфере передатчика
    if(data_start > data_end) {
        // данные "закольцованы" и адрес начала данных больше адреса конца данных
        data_space[0] = data_start - data_end;
        data_space[1] = 0;
    } else {
        data_space[0] = ARM_ETH_BUF_FULL_SIZE - data_end;
        data_space[1] = data_start - (uint16_t)ARM_ETH_BUF_BASE_X;
    }

    length_words = (length_bytes+3)>>2; // длина с в словах учётом выравнивания.

    // Информация о доступности места в буфере должна передаваться на TCP-уровень(инициатору передачи пакета)
    // либо необходима местная логика обработки данной ситуации.
    // если места в буфере больше, чем кол-во байт данных (+2 - учет 2х 32-разрядных информационных полей см даташит)
    //  if(((data_space[0]+data_space[1])>>2) >= (length_words +2)) {
        source = (uint32_t*)buf;
        dest = (uint32_t*)(ARM_ETH_BUF_BASE + data_end); // адрес в буфере для записи нового пакета данных

        *dest++ = (uint32_t)length_bytes; // заполнение поля управления передачи пакета (кол-во байт в пакете)
        eth_err_0022_correction();
        
        data_space[0] -= 4;     // соотв. свободное место буффера уменьшилось на 32-разрядное слово.

        if(dest >= (uint32_t*)(ARM_ETH_BUF_BASE + ARM_ETH_BUF_FULL_SIZE)) { // "закольцовка" 
            dest = (uint32_t*)ARM_ETH_BUF_BASE_X;
        }
        
        length_bytes = length_words << 2; // Выравнивание на случай если length_bytes не кратно слову
        if(length_bytes <= data_space[0]) {
            // кадр не надо "закольцовывать"
            for(i = 0; i < length_words; i++) {
                *dest++ = *source++;
            }
        } else {
            // кадр закольцован
            data_space[0] >>= 2;    // кол-во 32-разрядных слов
            for(i = 0; i < data_space[0]; i++) {
                *dest++ = *source++;
            }
            
            length_words -= data_space[0];
            dest = (uint32_t*)ARM_ETH_BUF_BASE_X;            
            
            eth_err_0022_correction();
            for(i = 0; i < length_words; i++) { 
                *dest++ = *source++;
            }
        }
        
        if(dest >= (uint32_t*)(ARM_ETH_BUF_BASE + ARM_ETH_BUF_FULL_SIZE)) { // "закольцовка буффера"
            dest = (uint32_t*)ARM_ETH_BUF_BASE_X;
        }
        //tx_status = dest;
        *dest++ = 0;    // обнуление слова за данными, в это слово записывается поле состояния передачи пакета(см даташит)
                        // теперь пакет заполнен 

        if(dest >= (uint32_t*)(ARM_ETH_BUF_BASE + ARM_ETH_BUF_FULL_SIZE)) { // "закольцовка буффера"
            dest = (uint32_t*)ARM_ETH_BUF_BASE_X;
        }
        ARM_ETH->X_TAIL = (uint16_t)((unsigned)dest);
        eth_err_0022_correction();
//    }
    // можно сохранять адрес tx_status, чтобы после отсыла пакета считать статус. 
#endif
}

// чтение кадра из буффера приемника          
uint32_t eth_read_frame (void *buf)
{
#ifdef ETH_BUFTYPE_FIFO
    uint32_t *dest = (uint32_t*)buf;
    uint32_t rx_status;
    uint32_t i, length_words, length_bytes;
    
//    if(ARM_ETH->STAT & ARM_ETH_R_COUNT(7)) { // буфер приемника не пуст
        rx_status = ARM_ETH_RX_FIFO;
        length_bytes = (ARM_ETH_PKT_LENGTH(rx_status) > ETH_MTU) ? ETH_MTU : ARM_ETH_PKT_LENGTH(rx_status);
        length_words = (length_bytes + 3) >> 2;

        for(i=0; i< length_words; i++) {
            *dest++ = ARM_ETH_RX_FIFO;
        }
        if(ARM_ETH->STAT & ARM_ETH_R_COUNT(7)) {
            ARM_ETH->STAT -= ARM_ETH_R_COUNT(1);    // минус один кадр
        }
        eth_err_0022_correction(); 
        //if(rx_status & ARM_ETH_PKT_LEN_ERR) debug_printf ("i %d stat %x",i, rx_status);
//    }
    return rx_status;
#else
    uint16_t data_start, data_end;  // start - адрес первого байта данных / end - адрес байта, следующего за последним байтом данных
    uint16_t data_space;
    uint16_t i, frame_length;       // кол-во несчитанных байт кадра
    uint32_t *source;               // источник данных
    uint32_t *dest;                 // приемник данных
    uint32_t rx_status=0;           // поле состояния приема пакета

    if(ARM_ETH->R_HEAD != ARM_ETH->R_TAIL) {
        data_start = ARM_ETH->R_HEAD;
        data_end   = ARM_ETH->R_TAIL;
        // R_TAIL меняется автоматически, в зависимости от длины данных,
        // указывает на пустую ячейку, следующую за данными(которые могут содержать несколько кадров).
        
        // количество байт данных в пакете, расчитанно из адреса начала данных и начала пустой области
        if(data_start < data_end) {
            data_space = data_end - data_start; // общее кол-во байтов (может включать несколько кадров)
        } else { 
            data_space = ARM_ETH_BUF_SIZE_R - data_start; // ситуация когда буффер "закольцован". вычисляется кол-во байтов до ARM_ETH_BUF_SIZE_R
        }
        source = (uint32_t*)(ARM_ETH_BUF_BASE + data_start);
        dest = ((uint32_t*)buf);
        
        rx_status = *source++;  // поле состояния приема кадра (количество байт данных в кадре, включая заголовок и FCS и статус)
        frame_length = (uint16_t)rx_status;

        data_space -= 4;  // отнимаются 4 считанных байта поля состояния приема пакета. остаётся "чистый" кадр;

        if(source >= (uint32_t*)(ARM_ETH_BUF_BASE + ARM_ETH_BUF_SIZE_R)) { // "кольцовка" буффера
            source = (uint32_t*)ARM_ETH_BUF_BASE;
        }

        frame_length = (frame_length+3)>>2; // +3 для выравнивания по слову (frame_length м.б. не кратен четырём)
        data_space >>= 2; 

        eth_err_0022_correction();
        if(frame_length <= data_space) {
            // считывание области R_HEAD....конец кадра
            for(i = 0; i < frame_length; i++) {
                *dest++ = *source++;
            }
        } else {
            // требуются два подхода в считывании (условие, когда кадр "закольцован" в буффере)
            // считывание области R_HEAD....ARM_ETH_BUF_SIZE_R
            for(i = 0; i < data_space ; i++) { 
                *dest++ = *source++;
            }

            // считывание области 0....(frame_length)
            source = (uint32_t*)ARM_ETH_BUF_BASE;
            frame_length -= data_space;
            for(i = 0; i < frame_length; i++) { 
                *dest++ = *source++;
            }
        }
        
        if(source >= (uint32_t*)(ARM_ETH_BUF_BASE + ARM_ETH_BUF_SIZE_R)) { // "кольцовка" буффера
            source = (uint32_t*)ARM_ETH_BUF_BASE;
        }
        ARM_ETH->R_HEAD = (uint16_t)((unsigned)source);  // указание свободной для приёма области (первая пустая ячейка после данного пакета)
    }
    if(ARM_ETH->STAT & ARM_ETH_R_COUNT(7)) {
        ARM_ETH->STAT -= ARM_ETH_R_COUNT(1);    // минус один кадр
    }
    eth_err_0022_correction();

    return rx_status;
#endif
}                               
#endif

// Write the packet to chip memory and start transmission.
// Deallocate the packet.
void eth_chip_transmit_packet(eth_t *u, buf_t *p)
{
    // Send the data from the buf chain to the interface,
    // one buf at a time. The size of the data in each
    // buf is kept in the ->len variable. 
    buf_t *q;
    uint8_t *buf = (uint8_t*) u->txbuf;
    
    for (q=p; q; q=q->next) { // перебор участков памяти буффера. в этих участках один кадр или несколько?
        // Copy the packet into the transmit buffer. 
        assert (q->len > 0);
//  debug_printf ("txcpy %08x <- %08x, %d bytes\n", buf, q->payload, q->len);
        memcpy (buf, q->payload, q->len);
        buf += q->len;
    }

    uint16_t len = p->tot_len;
    if(len < 60) {
        len = 60;
//  debug_printf ("txzero %08x, %d bytes\n", u->txbuf + p->tot_len, len - p->tot_len);
        memset (u->txbuf + p->tot_len, 0, len - p->tot_len);
    }
    eth_send_processing(u->txbuf, len);
    
    u->netif.out_packets++;
    u->netif.out_bytes += len;
//  debug_printf ("\nsend next packet\n");
//  debug_printf ("tx%d", len); buf_print_data (u->txbuf, p->tot_len);
}

// Do the actual transmission of the packet. The packet is contained
// in the pbuf that is passed to the function. This pbuf might be chained.
// Return 1 when the packet is succesfully queued for transmission.
// Or return 0 if the packet is lost.
bool_t eth_output(eth_t *u, buf_t *p, small_uint_t prio)
{  
    mutex_lock (&u->tx_lock);
    
    // Exit if link has failed 
    if((p->tot_len < 4/*18*/) || (p->tot_len > ETH_MTU) || (ARM_ETH->PHY_STAT & ARM_ETH_PHY_LED_LINK)) {
        u->netif.out_errors++;       
//  debug_printf ("eth_output: transmit %d bytes, link failed\n", p->tot_len);
        buf_free (p);
        mutex_unlock (&u->tx_lock);
        return 0;
    }
//  debug_printf ("eth_output: transmit %d bytes\n", p->tot_len);

    int ethBufVacant = 1;   // Для проверки наличия свободного места в буфере под новый кадр
#ifdef ETH_BUFTYPE_FIFO
    if(ARM_ETH->STAT & (ARM_ETH_X_FULL | ARM_ETH_X_AFULL | ARM_ETH_X_HALF)) {
//  debug_printf("eth phyBuf overfow\n");
        ethBufVacant = 0;
    }
#else 
    // Проверка есть ли место в буфере передатчика для нового пакета, 
    // сделано до извлечения нового пакета из очереди
    uint16_t data_space[2];
    uint16_t data_start = ARM_ETH->X_HEAD;
    uint16_t data_end = ARM_ETH->X_TAIL;
    uint16_t req_len = (p->tot_len < 60) ? 60+8 : p->tot_len+12; // 12 = 8+4. 8 - учёт 2х служебных слов. 4 - введено чтобы при полностью
                                                                 // заполненном буфере X-head и X-tail не были равны т.к. X-tail должен 
                                                                 // указывать на пустое слово, также требуется для FIFO буферов
    // количество свободных байт в буфере передатчика
    if(data_start > data_end) {
        // данные "закольцованы" и адрес начала данных больше адреса конца данных
        data_space[0] = data_start - data_end;
        data_space[1] = 0;
    } else {
        data_space[0] = ARM_ETH_BUF_FULL_SIZE - data_end;
        data_space[1] = data_start - (uint16_t)ARM_ETH_BUF_BASE_X;
    }
    if((data_space[0]+data_space[1]) < req_len) {
//  debug_printf("eth_output(): phyTxBuf overfow %d, req_len %d, \n", data_space[0]+data_space[1], req_len);
        ethBufVacant = 0;
    }
#endif

    if(buf_queue_is_empty(&u->outq) && ethBufVacant) {  
        // Отправка кадра.
        eth_chip_transmit_packet(u, p); 
        buf_free(p);
        mutex_unlock(&u->tx_lock); 
        return 1;
    }
    // Занято, ставим в очередь. 
//  debug_printf("tr %d\n",buf_queue_size(&u->outq)+p->tot_len);   
 #ifdef ETH_POLL_MODE
    while(buf_queue_is_full(&u->outq) || ((buf_queue_size(&u->outq)+p->tot_len) > ETH_OUTQ_SBYTES))
        mutex_wait(&u->tx_lock);
 #else 

    /* Занято, ставим в очередь. */
    #if 0
    if(buf_queue_is_full(&u->outq) || ((buf_queue_size(&u->outq)+p->tot_len) > ETH_OUTQ_SBYTES)) { 
        // Нет места в очереди: теряем пакет. 
        u->netif.out_discards++; 1
//  debug_printf("eth_output: overflow\n"); 
        buf_free(p);
        mutex_unlock(&u->tx_lock);
        return 0;
    }
    #else
    //while (buf_queue_is_full (&u->outq) || ((buf_queue_size(&u->outq)+p->tot_len) > ETH_OUTQ_SBYTES)) {
    while (buf_queue_is_full (&u->outq)) {
        mutex_wait (&u->tx_lock); 
    }
    #endif
 #endif
 
//  debug_printf("put in tx buf\n", p->tot_len);
    buf_queue_put(&u->outq, p);
    mutex_unlock(&u->tx_lock);
    return 1;
}

// Get a packet from input queue.
buf_t *eth_input(eth_t *u)
{
    buf_t *p;
    mutex_lock(&u->netif.lock);
    p = buf_queue_get(&u->inq);
    mutex_unlock(&u->netif.lock);
    return p;
}

// Setup MAC address.
void eth_set_address (eth_t *u, uint8_t *addr)
{
    mutex_lock (&u->netif.lock);
    memcpy (&u->netif.ethaddr, addr, 6);
    memcpy ((void*)ARM_ETH->MAC_ADDR, addr, 6);
//debug_printf ("UCADDR=%02x:%08x\n", MC_MAC_UCADDR_H, MC_MAC_UCADDR_L);
    eth_err_0022_correction();
    mutex_unlock (&u->netif.lock);
}

// Fetch the received packet from the network controller.
// Put it to input queue.
uint8_t eth_receive_frame(eth_t *u)
{
    uint32_t frame_status, frame_length;
    
#ifdef ETH_BUFTYPE_FIFO
    frame_status = eth_read_processing (u->rxbuf);  
    frame_length = ARM_ETH_PKT_LENGTH (frame_status);
#else
    frame_status = eth_rx_status();
    frame_length = (uint16_t)frame_status; // длина кадра = два мак адреса + тип кадра(длина) + поле данных + crc

    //если есть ошибки и длина больше стандарта и буффера
    if((frame_length < 18) || (frame_length > ETH_MTU)) { //|| (frame_status & ARM_ETH_R_CRC_ERR))// 18 - заголовки + crc с нулевым полем данных 
//      debug_printf ("eth_receive_data: failed, frame_status = %#08x, frame_length = %d\n",frame_status,frame_length);
        u->netif.in_errors++; 
        eth_rx_buffer_clr(); 
        //eth_restart_receiver();
        debug_printf("r_rst\n");
        return 0; 
    }
    eth_read_processing (u->rxbuf); 
#endif
//debug_printf ("eth_receive_data: ok, frame_status=%#08x, length %d bytes\n", frame_status, frame_length);

    //if(buf_queue_is_full(&u->inq) || ((buf_queue_size(&u->inq)+frame_length) >= ETH_INQ_SBYTES)) {
    if(buf_queue_is_full(&u->inq)) {
//debug_printf ("eth_receive_data: input overflow\n");
        u->netif.in_discards++;
        mutex_signal(&u->netif.lock, 0);
        return 0;
    }
    
    // Чтение кадра. Проводится до проверки переполнения очереди, т.к
    // лучше освобождать буфер приёмника, чтобы уменьшить вероятность возникновения
    // "ошибки буфера приёмника"
    //eth_read_processing(u->rxbuf);
    
    u->netif.in_packets++;
    u->netif.in_bytes += frame_length; 
        
    // Allocate a buf chain with total length 'len' 
    buf_t *p = buf_alloc(u->pool, frame_length, /*2*/0);
    if(! p) {
        // Could not allocate a buf - skip received frame
//debug_printf ("eth_receive_data: ignore packet - out of memory\n");
        u->netif.in_discards++;
        return 0;
    }
    // Copy the packet data. 
//debug_printf ("receive %08x <- %08x, %d bytes\n", p->payload, u->rxbuf, frame_length);
    memcpy (p->payload, u->rxbuf, frame_length);
    buf_queue_put(&u->inq, p);
    mutex_signal(&u->netif.lock, 0);
    return 1; 
//debug_printf ("[%d]", p->tot_len); buf_print_ethernet (p);
}

// Process a receive interrupt.
// Return nonzero when there was some activity.
uint16_t eth_handle_receive_interrupt (eth_t *u)
{
//debug_printf ("handle_receive_interrupt\n");
    uint16_t active = 0;
    //помимо IFR можно прочитать поле статуса eth_rx_status

 #ifdef ETH_POLL_MODE   // при использовании быстрого обработчика 
    u->ifr_reg = ARM_ETH->IFR & 0x00FF;
    ARM_ETH->IFR |= u->ifr_reg; // сброс флагов в регистре
    eth_err_0022_correction();
 #endif
   
    //if((status_ifr & (ARM_ETH_OVF)) || (ARM_ETH->STAT & ARM_ETH_R_FULL)) {
    if(u->ifr_reg & ARM_ETH_OVF) {
        u->rx_ovf++; 
//debug_printf("ethernet RX buffer overfull: IFR %08X STAT %08X\n", u->ifr_reg, ARM_ETH->STAT);
    }

    //todo	//не очень понятно помогает восстановлению контроль кол-ва кадров или мешает
    //WHILE затыкается быстрее если параллельно с основным тестом сыпать короткими очередями(500 посылок по 1500 байт)
    // на 90 Мбит/с, но дольше работает если слать поток на 8 Мбит/с, с IF всё наоборот 
    //while(R_Buff_Has_Eth_Frame()) 
    /*if(R_Buff_Has_Eth_Frame()) {  // если очереди урезаны до 1го буфера
        eth_receive_frame(u);    // декрементирует ARM_ETH_R_COUNT
        active++;
    }*/ 

    //if(R_Buff_Has_Eth_Frame()) {    // если очереди урезаны 
    while(R_Buff_Has_Eth_Frame()) { // если очередь достаточно длиная (в т.ч. внешнее ОЗУ)         
        eth_receive_frame(u);
        active++;
    }

    /*while(R_Buff_Has_Eth_Frame() && eth_receive_frame(u))
        active++;*/
    return active;
}

// Process a transmit interrupt: fast handler.
// Return 1 when we are expecting the hardware interrupt.
void eth_handle_transmit_interrupt(eth_t *u)
{  // В некоторых случаях прерывание пропускается
    uint16_t status_phy = ARM_ETH->PHY_STAT;
 
 #ifdef ETH_POLL_MODE   // при использовании быстрого обработчика 
    u->ifr_reg = ARM_ETH->IFR & 0x1F00;
    ARM_ETH->IFR |= u->ifr_reg; // сброс флагов
    eth_err_0022_correction();
 #endif

    // подсчёт коллизии. 
    if((u->ifr_reg & ARM_ETH_LC) || (status_phy & ARM_ETH_PHY_COL))
        u->netif.out_collisions++;

    if(u->ifr_reg & ARM_ETH_XF_ERR) {
        u->netif.out_errors++;
        //eth_restart_transmitter(); 
    }

    int ethBufVacant = 1;   // Для проверки наличия свободного места в буфере под новый кадр
#ifdef ETH_BUFTYPE_FIFO
    if(ARM_ETH->STAT & (ARM_ETH_X_FULL | ARM_ETH_X_AFULL | ARM_ETH_X_HALF)) {
        ethBufVacant = 0;
    }
#else 
    // Проверка есть ли место в буфере передатчика для нового пакета, 
    // д.б. сделана до извлечения нового пакета из очереди
    uint16_t data_space[2];
    uint16_t data_start = ARM_ETH->X_HEAD;
    uint16_t data_end = ARM_ETH->X_TAIL;
    uint16_t packLen = 0xFFF7; // 0xFFF7 = 0xFFFF-8 учёт дальнейшего сложения
    
    // Проверка текущего элемента очереди на общую длину (если он существует).
    buf_t *curBuf = *buf_queue_last(&u->outq);
    if (curBuf) {
        packLen = (*buf_queue_last(&u->outq))->tot_len; 
//  debug_printf("eth_irq: packLen %d\n", packLen);
    }
    uint16_t req_len = (packLen < 60) ? 60+8 : packLen+12;  // 12 = 8+4. 8 - учёт 2х служебных слов. 4 - введено чтобы при полностью
                                                            // заполненном буфере X-head и X-tail не были равны т.к. X-tail должен 
                                                            // указывать на пустое слово, также требуется для FIFO буферов
    if(data_start > data_end) {
        // данные "закольцованы" и адрес начала данных больше адреса конца данных
        data_space[0] = data_start - data_end;
        data_space[1] = 0;
    } else {
        data_space[0] = ARM_ETH_BUF_FULL_SIZE - data_end;
        data_space[1] = data_start - (uint16_t)ARM_ETH_BUF_BASE_X;
    }
    if((data_space[0]+data_space[1]) < req_len) {
        ethBufVacant = 0;
    }
#endif
    if(! ethBufVacant) {
//  debug_printf("eth_irq: phyTxBuf ovf %d, req_len %d, \n", data_space[0]+data_space[1], req_len);
        return;
    }
    
    //  mutex_signal (&eth_interrupt_mutex, 0);
    eth_err_0022_correction();
    // Извлекаем следующий пакет из очереди. 
    buf_t *p = buf_queue_get (&u->outq);
    if(! p) {
//  debug_printf ("eth tx irq: done, STATUS_PHY = %08x\n", status_phy);
        return;
    } 
    eth_chip_transmit_packet(u, p);
    buf_free (p);
    // Передаётся следующий пакет.
//  debug_printf ("eth tx irq: send next packet, STATUS_PHY = %08x\n", status_phy);
}

netif_interface_t eth_interface = {
    (bool_t (*) (netif_t*, buf_t*, small_uint_t))   eth_output,
    (buf_t *(*) (netif_t*))                         eth_input,
    (void (*)   (netif_t*, uint8_t*))               eth_set_address
};

// Set up the network interface.
void eth_init(eth_t *u, const char *name, int prio, mem_pool_t *pool, struct _arp_t *arp, const uint8_t *macaddr, uint8_t phy_mode)
{
    u->netif.interface = &eth_interface;
    u->netif.name = name;
    u->netif.arp = arp;
    u->netif.mtu = 1500;
    u->netif.type = NETIF_ETHERNET_CSMACD;
    //u->netif.bps = 100000000;
    u->netif.out_bytes = 0;
    memcpy (&u->netif.ethaddr, macaddr, 6);

    u->pool = pool;
    eth_err_0022_correction();
    u->rxbuf = (uint8_t*) (eth_rx_buf);      
    u->txbuf = (uint8_t*) (eth_tx_buf);
    u->rxbuf_physaddr = ARM_ETH_BUF_BASE;
    u->txbuf_physaddr = ARM_ETH_BUF_BASE + ARM_ETH_BUF_SIZE_R;
    u->phy = ARM_ETH_PHY_ADDRESS;
    buf_queue_init(&u->inq, u->inqdata, sizeof (u->inqdata));
    buf_queue_init(&u->outq, u->outqdata, sizeof (u->outqdata));
    u->ifr_reg = 0;

    // Initialize hardware. Для работы с режимом NO AUTO требуется настройка сети(сетевой карты на соотв режим)
    // sudo ethtool -s eth1 duplex full speed 10 autoneg off
    // где eth1-сетевой интерфейс для BE1
    eth_controller_init(macaddr, phy_mode);
 
 #ifndef ETH_POLL_MODE
    // Enable interrupts
    memset(&u->irq_rx, 0, sizeof(u->irq_rx));
    //u->irq_rx.mask1 = ARM_ETH_RF_OK | ARM_ETH_OVF | ARM_ETH_MISSED_F; 
    u->irq_rx.mask1 = ARM_ETH_RF_OK | ARM_ETH_OVF; 
    u->irq_rx.handler_arg = u;
    u->irq_rx.handler_lock = &u->netif.lock;
    ARM_ETH->IMR |= u->irq_rx.mask1;
    
    memset (&u->irq_tx, 0, sizeof(u->irq_tx));
    u->irq_tx.mask1 = ARM_ETH_XF_OK | ARM_ETH_XF_ERR | ARM_ETH_XF_UNDF;
    //u->irq_tx.mask1 = ARM_ETH_XF_OK;
    u->irq_tx.handler_arg = u;
    u->irq_tx.handler_lock = &u->tx_lock;
    ARM_ETH->IMR |= u->irq_tx.mask1;  
 #endif
    eth_err_0022_correction();
}
#ifdef ETH_USE_DMA
ARRAY (stack_dma, 1000);
void dma_interrupt_task (void *arg) 
{
    //eth_t *u = arg; 

    mutex_lock_irq (&dma_interrupt_mutex, DMA_IRQn, 0, 0);  
    for(;;)
    {   //debug_printf ("irqDma\n");
        mutex_wait (&dma_interrupt_mutex); 

        if(waitDma) {
            mutex_signal (&dma_mutex, 0);
            waitDma=0;
            //debug_printf ("waitDma=0\n");
        }
    }
}

int dma_interrupt (void *arg) 
{
//    debug_printf ("dmaIRQ\n");
    if(waitDma) {
        if ((dma_conf[ETH_DMA_CHN_TX].CONTROL & (ARM_DMA_TRANSFERS(1024) | ARM_DMA_AUTOREQ)) == 0) {
            debug_printf ("waitDma=%d, cf=%d\n", waitDma, cyclFrame);
            waitDma=0;
        }
    }
    arch_intr_allow(DMA_IRQn);
    return 1;
}
//void dma_interrupt_task (void *arg) __attribute__ ((weak, alias ("__dma_interrupt_task")));
#endif

// Запуск обработчика прерываний, без быстрого обработчика
void create_eth_interrupt_task (eth_t *u, int prio, void *stack, int stacksz)
{
    if(! eth_interrupt_task_created) {
        eth_task = task_create (eth_interrupt_task, u, "eth_int", prio, stack, stacksz);
#ifdef ETH_USE_DMA
        // IRQ 1 (MIL_STD_1553B1_IRQn) никогда не запрещается
//        extern uint32_t mask_intr_disabled;
//        mask_intr_disabled = ~(1<<DMA_IRQn);
//        ARM_NVIC_IPR(DMA_IRQn/4) = 0x40400040;
        
        // ППК Миланд (Tatyana):
        // Небольшое дополнение к выше сказанному, еще необходимо подать частоту на блоки инициирующие ложные запросы, это блоки SSP1,2.
        // http://forum.milandr.ru/viewtopic.php?t=234&start=15
        // По факту и SSP3
        ARM_RSTCLK->PER_CLOCK |= ARM_PER_CLOCK_SSP1 | ARM_PER_CLOCK_SSP2 | ARM_PER_CLOCK_SSP3;
        ARM_RSTCLK->PER_CLOCK &= ~(ARM_PER_CLOCK_SSP1 | ARM_PER_CLOCK_SSP2 | ARM_PER_CLOCK_SSP3);
        //task_create (dma_interrupt_task, u, "eth_int", prio-10, stack_dma, sizeof(stack_dma));
        mutex_attach_irq (&dma_interrupt_mutex, DMA_IRQn, dma_interrupt, 0);
#endif
        eth_interrupt_task_created = 1;
    }
}

// Interrupt task. Работа без быстрого обработчика
void eth_interrupt_task (void *arg)
{
    eth_t *u = arg; 
    mutex_lock_irq (&eth_interrupt_mutex, ETHERNET_IRQn, 0, 0);  
    for (;;)
    {
        mutex_wait (&eth_interrupt_mutex);  
        u->ifr_reg = ARM_ETH->IFR;
        ARM_ETH->IFR = u->ifr_reg;
        
        if((u->ifr_reg & u->irq_rx.mask1) || R_Buff_Has_Eth_Frame ()) { 
            mutex_lock (&u->netif.lock);
            eth_handle_receive_interrupt (u);
            mutex_unlock (&u->netif.lock);
        }
        if((u->ifr_reg & u->irq_tx.mask1)) { 
            mutex_lock (&u->tx_lock); 
            eth_handle_transmit_interrupt (u);
            mutex_signal (&u->tx_lock, 0);
            mutex_unlock (&u->tx_lock);
        }
    }
}
