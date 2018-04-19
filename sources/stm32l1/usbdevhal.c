#include <loadable/uos-interface.h>
#include "usbdevhal.h"

extern uos_loadable_t *uos;

ARRAY (hp_stack, 1000);
ARRAY (lp_stack, 1000);

#if 0
static void dump (const void *addr, int size)
{
    const unsigned char *p = addr;
    int i;
    for (i = 0; i < size; ++i)
        debug_printf ("%02X ", p[i]);
    debug_printf ("\n");
}
#endif

static void stm32l1_set_addr (unsigned addr, void *arg)
{
    USB->DADDR = USB_EF | USB_ADD(addr);
}

static void stm32l1_ep_attr (unsigned ep, int dir, unsigned attr, int max_size, int interval, void *arg)
{
//debug_printf ("ep_attr, ep = %d, dir = %d, attr = 0x%X, max_size = %d\n", ep, dir, attr, max_size);
    stm32l1_usbdev_t *u = arg;
    unsigned cur_epr = u->nb_active_eps;
    
    lassert(uos, cur_epr + 1 <= USBDEV_MAX_EP_NB);

    switch (attr & EP_ATTR_TRANSFER_MASK) {
    case EP_ATTR_CONTROL:
        lassert(uos, u->free_pkt_mem_offset + (max_size << 1) <= 1024);
        u->btable[cur_epr].ADDR_TX = (u->free_pkt_mem_offset >> 1);
        u->free_pkt_mem_offset += max_size;
        u->btable[cur_epr].ADDR_RX = (u->free_pkt_mem_offset >> 1);
        u->free_pkt_mem_offset += max_size;
        if (max_size < 32)
            u->btable[cur_epr].COUNT_RX = USB_NUM_BLOCK(max_size / 2);
        else
            u->btable[cur_epr].COUNT_RX = USB_BL_SIZE | USB_NUM_BLOCK(max_size / 32 - 1);
        USB->EPR[cur_epr] = USB_STAT_RX_NAK | USB_STAT_TX_NAK | 
            USB_EP_TYPE_CONTROL | USB_EA(ep);
        u->ep_in_to_epr[ep] = cur_epr;
        u->ep_out_to_epr[ep] = cur_epr;
    break;
    case EP_ATTR_ISOCH:
        lassert(uos, u->free_pkt_mem_offset + (max_size << 1) <= 1024);
        u->btable[cur_epr].ADDR_TX = (u->free_pkt_mem_offset >> 1);
        u->free_pkt_mem_offset += max_size;
        u->btable[cur_epr].ADDR_RX = (u->free_pkt_mem_offset >> 1);
        u->free_pkt_mem_offset += max_size;
        if (dir == USBDEV_DIR_IN) {
            //USB->EPR[cur_epr] = USB_STAT_TX_NAK | USB_EP_TYPE_BULK | 
            //    USB_DBL_BUF | USB_EA(ep) | USB_DTOG_RX;
            USB->EPR[cur_epr] = USB_EP_TYPE_ISO;
            u->ep_in_to_epr[ep] = cur_epr;
        } else {
            if (max_size < 32) {
                u->btable[cur_epr].COUNT_TX = USB_NUM_BLOCK(max_size / 2);
                u->btable[cur_epr].COUNT_RX = USB_NUM_BLOCK(max_size / 2);
            } else {
                u->btable[cur_epr].COUNT_TX = USB_BL_SIZE | USB_NUM_BLOCK(max_size / 32 - 1);
                u->btable[cur_epr].COUNT_RX = USB_BL_SIZE | USB_NUM_BLOCK(max_size / 32 - 1);
            }
            USB->EPR[cur_epr] = USB_STAT_RX_NAK | USB_EP_TYPE_ISO | 
                USB_DBL_BUF | USB_EA(ep) | USB_DTOG_TX;
            u->ep_out_to_epr[ep] = cur_epr;
        }
    break;
    case EP_ATTR_BULK:
        lassert(uos, u->free_pkt_mem_offset + (max_size << 1) <= 1024);
        u->btable[cur_epr].ADDR_TX = (u->free_pkt_mem_offset >> 1);
        u->free_pkt_mem_offset += max_size;
        u->btable[cur_epr].ADDR_RX = (u->free_pkt_mem_offset >> 1);
        u->free_pkt_mem_offset += max_size;
        if (dir == USBDEV_DIR_IN) {
            //USB->EPR[cur_epr] = USB_STAT_TX_NAK | USB_EP_TYPE_BULK | 
            //    USB_DBL_BUF | USB_EA(ep) | USB_DTOG_RX;
            USB->EPR[cur_epr] = USB_EP_TYPE_BULK;
            u->ep_in_to_epr[ep] = cur_epr;
        } else {
            if (max_size < 32) {
                u->btable[cur_epr].COUNT_TX = USB_NUM_BLOCK(max_size / 2);
                u->btable[cur_epr].COUNT_RX = USB_NUM_BLOCK(max_size / 2);
            } else {
                u->btable[cur_epr].COUNT_TX = USB_BL_SIZE | USB_NUM_BLOCK(max_size / 32 - 1);
                u->btable[cur_epr].COUNT_RX = USB_BL_SIZE | USB_NUM_BLOCK(max_size / 32 - 1);
            }
            USB->EPR[cur_epr] = USB_STAT_RX_NAK | USB_EP_TYPE_BULK | 
                USB_DBL_BUF | USB_EA(ep) | USB_DTOG_TX;
            u->ep_out_to_epr[ep] = cur_epr;
        }
    break;
    case EP_ATTR_INTR:
        lassert(uos, u->free_pkt_mem_offset + max_size <= 1024);
        if (dir == USBDEV_DIR_IN) {
            u->btable[cur_epr].ADDR_TX = (u->free_pkt_mem_offset >> 1);
            USB->EPR[cur_epr] = USB_STAT_TX_NAK | USB_EP_TYPE_INTERRUPT | USB_EA(ep);
            u->ep_in_to_epr[ep] = cur_epr;
        } else {
            u->btable[cur_epr].ADDR_RX = (u->free_pkt_mem_offset >> 1);
            if (max_size < 32)
                u->btable[cur_epr].COUNT_RX = USB_NUM_BLOCK(max_size / 2);
            else
                u->btable[cur_epr].COUNT_RX = USB_BL_SIZE | USB_NUM_BLOCK(max_size / 32 - 1);
            USB->EPR[cur_epr] = USB_STAT_RX_NAK | USB_EP_TYPE_INTERRUPT | USB_EA(ep);
            u->ep_out_to_epr[ep] = cur_epr;
        }
        u->free_pkt_mem_offset += max_size;
    break;
    }
    
    u->max_size[cur_epr] = max_size;
    u->nb_active_eps = ++cur_epr;
}

static void stm32l1_ep_wait_out (unsigned ep, int ack, void *arg)
{
//debug_printf ("ep_wait_out, ep = %d\n", ep);
    stm32l1_usbdev_t *u = arg;
    unsigned epr = u->ep_out_to_epr[ep];
    if (!(USB->EPR[epr] & USB_EP_TYPE_CTRL_INTR_MASK)) {
        if ((USB->EPR[epr] & USB_STAT_RX_MASK) != USB_STAT_RX_VALID) {
            USB->EPR[epr] = (USB->EPR[epr] & (USB_EPR_RW_MASK | USB_STAT_RX_MASK)) ^ USB_STAT_RX_VALID;
        }
    } else {
        USB->EPR[epr] = (USB->EPR[epr] & (USB_EPR_RW_MASK | USB_STAT_RX_MASK)) ^ USB_STAT_RX_VALID;
    }
}

static void stm32l1_ep_wait_in (unsigned ep, int pid, const void *data, int size, int last, void *arg)
{
//debug_printf ("ep_wait_in, ep = %d, pid = %d, data @ %p, size = %d, last = %d\n", ep, pid, data, size, last);
    stm32l1_usbdev_t *u = arg;
    volatile uint32_t *packet;
    const uint16_t *p = data;
    int i;
    unsigned epr = u->ep_in_to_epr[ep];
    int bulk_iso_ep = !(USB->EPR[epr] & USB_EP_TYPE_CTRL_INTR_MASK);
    
    if (bulk_iso_ep) {
        if (USB->EPR[epr] & USB_DTOG_TX) {
            packet = (volatile uint32_t *)(USB_PACKET_BUF + (u->btable[epr].ADDR_RX << 1));
            u->btable[epr].COUNT_RX = size;
        } else {
            packet = (volatile uint32_t *)(USB_PACKET_BUF + (u->btable[epr].ADDR_TX << 1));
            u->btable[epr].COUNT_TX = size;
        }
    } else {
        packet = (volatile uint32_t *)(USB_PACKET_BUF + (u->btable[epr].ADDR_TX << 1));
        u->btable[epr].COUNT_TX = size;
    }
    
    for (i = 0; i < ((size + 1) >> 1); ++i)
        *packet++ = *p++;
        
    if (bulk_iso_ep) {
        if (~u->bulk_iso_active & (1 << epr)) {
            if ((USB->EPR[epr] & USB_EP_TYPE_MASK) == USB_EP_TYPE_BULK) {
                USB->EPR[epr] = USB_STAT_TX_VALID | USB_EP_TYPE_BULK | 
                    USB_DBL_BUF | USB_EA(ep) | USB_DTOG_RX;
            } else {
                USB->EPR[epr] = USB_STAT_TX_VALID | USB_EP_TYPE_ISO | 
                    USB_DBL_BUF | USB_EA(ep);
            }
            u->bulk_iso_active |= (1 << epr);
        } else {
            USB->EPR[epr] = (USB->EPR[epr] & USB_EPR_RW_MASK) | USB_DTOG_RX;
        }
    } else {
        USB->EPR[epr] = (USB->EPR[epr] & (USB_EPR_RW_MASK | USB_STAT_TX_MASK)) ^ USB_STAT_TX_VALID;
    }
}

static void stm32l1_ep_stall (unsigned ep, int dir, void *arg)
{
//debug_printf ("STALL EP%d\n", ep);
    stm32l1_usbdev_t *u = arg;
    unsigned epr;
    
    if (dir == USBDEV_DIR_IN)
         epr = u->ep_in_to_epr[ep];
    else epr = u->ep_out_to_epr[ep];
    
    USB->EPR[epr] = (USB->EPR[epr] & USB_EPR_RW_MASK) |
        ((USB->EPR[epr] & USB_STAT_RX_MASK) ^ USB_STAT_RX_STALL) |
        ((USB->EPR[epr] & USB_STAT_TX_MASK) ^ USB_STAT_TX_STALL);
}

static int stm32l1_in_avail_bytes (unsigned ep, void *arg)
{
    stm32l1_usbdev_t *u = arg;
    unsigned epr = u->ep_in_to_epr[ep];
    
    if (!(USB->EPR[epr] & USB_EP_TYPE_CTRL_INTR_MASK)) {
        // BULK or ISOCHRONOUS
        if (USB_GET_DTOG_RX(USB->EPR[epr]) ^ USB_GET_DTOG_TX(USB->EPR[epr])) {
            //debug_printf("avail = 0\n");
            return 0;
        } else {
            //debug_printf("avail = %d\n", u->max_size[epr]);
            return u->max_size[epr];
        }
    } else {
        // CONTROL or INTERRUPT
        if ((USB->EPR[epr] & USB_STAT_TX_MASK) == USB_STAT_TX_VALID) {
            //debug_printf("avail = 0\n");
            return 0;
        }
        else {
            //debug_printf("avail = %d\n", u->max_size[epr]);
            return u->max_size[epr];
        }
    }
}

static usbdev_hal_t hal = {
    .set_addr       = stm32l1_set_addr,
    .ep_attr        = stm32l1_ep_attr,
    .ep_wait_out    = stm32l1_ep_wait_out,
    .ep_wait_in     = stm32l1_ep_wait_in,
    .ep_stall       = stm32l1_ep_stall,
    .in_avail       = stm32l1_in_avail_bytes
};

static void usb_lp_interrupt_task (void *arg)
{
    stm32l1_usbdev_t *u = arg;
    unsigned istr;
    unsigned bytes;
    unsigned i, ep;
    unsigned epr;
    unsigned epr_value;
    volatile uint32_t *packet;
    
    _UC(uos) mutex_lock_irq (&u->lock, IRQ_USB_LP, 0, 0);
    
    _UC(uos) usbdevhal_reset(u->usbdev);
    
    SYSCFG->PMC |= SYSCFG_USB_PU;
    
    for (;;) {
        _UC(uos) mutex_wait(&u->lock);
        
        istr = USB->ISTR & USB->CNTR;
        //debug_printf("lp interrupt, ISTR = %04X\n", USB->ISTR);
        if (istr == 0)
            continue;

        if (istr & USB_RESET) {
            //debug_printf("=== reset\n");
                
            USB->EPR[0] = USB_STAT_RX_VALID | USB_STAT_TX_NAK | USB_EP_TYPE_CONTROL;
            if (USBDEV_EP0_MAX_SIZE < 32)
                u->btable[0].COUNT_RX = USB_NUM_BLOCK(USBDEV_EP0_MAX_SIZE / 2);
            else
                u->btable[0].COUNT_RX = USB_BL_SIZE | USB_NUM_BLOCK(USBDEV_EP0_MAX_SIZE / 32 - 1);
            USB->DADDR = USB_EF;
            
            u->nb_active_eps = 1;
            u->bulk_iso_active = 0;
            u->free_pkt_mem_offset = EP0_RX_BUF_OFFSET + USBDEV_EP0_MAX_SIZE * 2;
            
            _UC(uos) usbdevhal_reset(u->usbdev);
            
            USB->ISTR = ~USB_RESET;
            
            continue;
        }
        
        if (istr & USB_CTR) {
            for (epr = 0; epr < u->nb_active_eps; ++epr) {
                epr_value = USB->EPR[epr];
                ep = USB_EA(epr_value);

                if (epr_value & USB_CTR_TX) {
                    if (! (epr_value & USB_EP_TYPE_CTRL_INTR_MASK)) {
                        // BULK or ISOCHRONOUS
                        if (epr_value & USB_DTOG_TX) {
                            _UC(uos) usbdevhal_in_done (u->usbdev, ep, u->btable[epr].COUNT_RX);
                        } else {
                            _UC(uos) usbdevhal_in_done (u->usbdev, ep, u->btable[epr].COUNT_TX);
                        }
                    } else {
                        _UC(uos) usbdevhal_in_done (u->usbdev, ep, u->btable[epr].COUNT_TX);
                    }
                }
                
                if (epr_value & USB_CTR_RX) {
                    if (! (epr_value & USB_EP_TYPE_CTRL_INTR_MASK)) {
                        // BULK or ISOCHRONOUS
                        if (epr_value & USB_DTOG_RX) {
                            bytes = USB_GET_COUNT_RX(u->btable[epr].COUNT_TX);
                            packet = (volatile uint32_t *)(USB_PACKET_BUF + (u->btable[epr].ADDR_TX << 1));
                        } else {
                            bytes = USB_GET_COUNT_RX(u->btable[epr].COUNT_RX);
                            packet = (volatile uint32_t *)(USB_PACKET_BUF + (u->btable[epr].ADDR_RX << 1));
                        }
                        for (i = 0; i < ((bytes + 1) >> 1); ++i)
                            u->rx_buf[i] = packet[i];
                        USB->EPR[epr] = (epr_value & USB_EPR_RW_MASK) | USB_DTOG_TX;
                        _UC(uos) usbdevhal_out_done (u->usbdev, ep, USBDEV_TRANSACTION_OUT, u->rx_buf, bytes);
                    } else {
                        //debug_printf("--r %d, %04X\n", ep, u->btable[epr].COUNT_RX);
                        bytes = USB_GET_COUNT_RX(u->btable[epr].COUNT_RX);
                        packet = (volatile uint32_t *)(USB_PACKET_BUF + (u->btable[epr].ADDR_RX << 1));
                        for (i = 0; i < ((bytes + 1) >> 1); ++i)
                            u->rx_buf[i] = packet[i];
                        if (epr_value & USB_SETUP) {
                            _UC(uos) usbdevhal_out_done (u->usbdev, ep, USBDEV_TRANSACTION_SETUP, u->rx_buf, bytes);
                        } else {
                            _UC(uos) usbdevhal_out_done (u->usbdev, ep, USBDEV_TRANSACTION_OUT, u->rx_buf, bytes);
                        }
                    }
                }
            }
        }
    }
}


void stm32l1_usbdev_init (stm32l1_usbdev_t *u, usbdev_t *owner, int io_prio, mem_pool_t *pool)
{
    u->usbdev = owner;
    u->mem = pool;
    
    _UC(uos) usbdevhal_bind (owner, &hal, u, &u->lock);

    RCC->APB1ENR |= RCC_USBEN;
    RCC->APB2ENR |= RCC_SYSCFGEN;
    asm volatile ("dmb");
    
    USB->CNTR = USB_FRES;   // removing PWDN bit
    _UC(uos) udelay(1);     // waiting for analog part to calm down
    USB->CNTR = 0;          // removing reset
    USB->ISTR = 0;          // clearing all spurious interrupts during startup
    
    USB->CNTR = USB_CTRM | USB_RESETM;
    
    u->btable = (USB_BTABLE_item_t *) USB_PACKET_BUF;
    u->btable[0].ADDR_TX = EP0_TX_BUF_OFFSET / 2;
    u->btable[0].ADDR_RX = EP0_RX_BUF_OFFSET / 2;
    u->free_pkt_mem_offset = EP0_RX_BUF_OFFSET + USBDEV_EP0_MAX_SIZE * 2;
    u->max_size[0] = USBDEV_EP0_MAX_SIZE;
    USB->BTABLE = 0;
        
    _UC(uos) task_create (usb_lp_interrupt_task, u, "usb_lp", io_prio, lp_stack, sizeof (lp_stack));
}
