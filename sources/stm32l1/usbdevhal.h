#ifndef __STM32L1_USB_DEV_HAL_H__
#define __STM32L1_USB_DEV_HAL_H__

#include <usb/usbdev.h>
#include <mem/mem.h>

#ifndef USBDEV_MAX_EP_NB
#define USBDEV_MAX_EP_NB    8
#endif

#define EP0_TX_BUF_OFFSET   (USBDEV_MAX_EP_NB * sizeof(USB_BTABLE_item_t))
#define EP0_RX_BUF_OFFSET   (EP0_TX_BUF_OFFSET + USBDEV_EP0_MAX_SIZE * 2)

typedef struct _stm32l1_usbdev_t
{
    mutex_t             lock;
    usbdev_t *          usbdev;
    mem_pool_t *        mem;
    
    USB_BTABLE_item_t * btable;
    
    uint16_t            rx_buf [EP_MAX_PKT_BULK_FS / 2];
    
    unsigned            nb_active_eps;
    unsigned            free_pkt_mem_offset;
    int                 max_size[USBDEV_MAX_EP_NB];
    uint16_t            bulk_iso_active;
    
    uint8_t             ep_in_to_epr[USBDEV_MAX_EP_NB];
    uint8_t             ep_out_to_epr[USBDEV_MAX_EP_NB];
} stm32l1_usbdev_t;

typedef struct _ep_state_t {
    uint8_t max_size[2];
    int8_t  last_in_bytes;
    uint8_t busy;
} ep_state_t;


void stm32l1_usbdev_init (stm32l1_usbdev_t *u, usbdev_t *owner, int io_prio, mem_pool_t *pool);

#endif // __STM32L1_USB_DEV_HAL_H__
