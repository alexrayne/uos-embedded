#include <runtime/lib.h>
#include <kernel/uos.h>
#include <mem/mem.h>
#include <usb/usbdev.h>
#include <usb/usb_struct.h>
#include <usb/usb_const.h>
#include <timer/timer.h>
#include <kernel/internal.h>

// Требуемые типы конечных точек. Могут быть значения:
// EP_ATTR_INTR, EP_ATTR_BULK, EP_ATTR_ISOCH
#define EP1_TYPE    EP_ATTR_INTR
#define EP2_TYPE    EP_ATTR_INTR
#define EP3_TYPE    EP_ATTR_BULK
#define EP4_TYPE    EP_ATTR_BULK

// Требуемые размеры конечных точек
#define EP1_SIZE    64
#define EP2_SIZE    64
#define EP3_SIZE    64
#define EP4_SIZE    64


#define USB_IRQ     0

ARRAY (task, 1000);
usbdev_t usb;
mem_pool_t pool;
mutex_t usb_lock;
timer_t timer;

uint8_t buf[8192] __attribute__((aligned(8)));

static const usb_dev_desc_t device_descriptor = {
    sizeof (usb_dev_desc_t),
    USB_DESC_TYPE_DEVICE,
    0x0100,
    0xFF,
    0,
    0xFF,
    USBDEV_EP0_MAX_SIZE,
    0x0111,
    0x0001,
    0x0010,
    1,
    2,
    3,
    1
};


// Language code string descriptor
typedef struct __attribute__ ((packed)) _lang_id_desc_t {
    unsigned char bLength;
    unsigned char bDescriptorType;
    uint16_t string [1];
} lang_id_desc_t;

// Manufacturer string descriptor
typedef struct __attribute__ ((packed)) _manuf_id_desc_t {
    unsigned char bLength;
    unsigned char bDescriptorType;
    uint16_t string [8];
} manuf_id_desc_t;

// Manufacturer string descriptor
typedef struct __attribute__ ((packed)) _prod_id_desc_t {
    unsigned char bLength;
    unsigned char bDescriptorType;
    uint16_t string [8];
} prod_id_desc_t;

// Manufacturer and product string descriptor
typedef struct __attribute__ ((packed)) _serial_id_desc_t {
    unsigned char bLength;
    unsigned char bDescriptorType;
    uint16_t string [4];
} serial_id_desc_t;

// Configuration ID string descriptor
typedef struct __attribute__ ((packed)) _conf_id_desc_t {
    unsigned char bLength;
    unsigned char bDescriptorType;
    uint16_t string [6];
} conf_id_desc_t;


typedef struct __attribute__ ((packed)) _this_conf_desc_t
{
    usb_conf_desc_t     conf;
    usb_iface_desc_t    iface;
    usb_ep_desc_t       ep_in;
    usb_ep_desc_t       ep_out;
    usb_ep_desc_t       ep3;
    usb_ep_desc_t       ep4;
} this_conf_desc_t;

typedef struct __attribute__ ((packed)) _string_desc_t
{
    lang_id_desc_t      lang_id;
    manuf_id_desc_t     manuf_id;
    prod_id_desc_t      prod_id;
    serial_id_desc_t    ser_id;
    conf_id_desc_t      conf_id;
} string_desc_t;

static const this_conf_desc_t config_descriptor = {
    // Configuration descriptor
    {
        .bLength            = sizeof (usb_conf_desc_t),
        .bDescriptorType    = USB_DESC_TYPE_CONFIG,
        .wTotalLength       = sizeof (this_conf_desc_t),
        .bNumInterfaces     = 1,
        .bConfigurationValue= 1,
        .iConfiguration     = 4,
        .bmAttributes       = USB_CONF_ATTR_REQUIRED | USB_CONF_ATTR_SELF_PWR,
        .bMaxPower          = 50,
    },

    // Interface descriptor
    {
        .bLength            = sizeof (usb_iface_desc_t),
        .bDescriptorType    = USB_DESC_TYPE_IFACE,
        .bInterfaceNumber   = 0,
        .bAlternateSettings = 0,
        .bNumEndpoints      = 4,
        .bInterfaceClass    = 0xFF,
        .bInterfaceSubClass = 1,
        .bInterfaceProtocol = 0xFF,
        .iInterface         = 0
    },

    // Endpoint1 IN descriptor
    {
        .bLength            = sizeof (usb_ep_desc_t),
        .bDescriptorType    = USB_DESC_TYPE_ENDPOINT,
        .bEndpointAddress   = EP_IN_NUMBER(1),
        .bmAttributes       = EP1_TYPE | EP_ATTR_NO_SYNC | EP_ATTR_DATA,
        .wMaxPacketSize     = EP1_SIZE,
        .bInterval          = 1
    },

    // Endpoint2 OUT descriptor
    {
        .bLength            = sizeof (usb_ep_desc_t),
        .bDescriptorType    = USB_DESC_TYPE_ENDPOINT,
        .bEndpointAddress   = EP_OUT_NUMBER(2),
        .bmAttributes       = EP2_TYPE | EP_ATTR_NO_SYNC | EP_ATTR_DATA,
        .wMaxPacketSize     = EP2_SIZE,
        .bInterval          = 1
    },

    // Endpoint3 IN descriptor
    {
        .bLength            = sizeof (usb_ep_desc_t),
        .bDescriptorType    = USB_DESC_TYPE_ENDPOINT,
        .bEndpointAddress   = EP_IN_NUMBER(3),
        .bmAttributes       = EP3_TYPE | EP_ATTR_NO_SYNC | EP_ATTR_DATA,
        .wMaxPacketSize     = EP3_SIZE,
        .bInterval          = 1
    },

    // Endpoint4 OUT descriptor
    {
        .bLength            = sizeof (usb_ep_desc_t),
        .bDescriptorType    = USB_DESC_TYPE_ENDPOINT,
        .bEndpointAddress   = EP_OUT_NUMBER(4),
        .bmAttributes       = EP4_TYPE | EP_ATTR_NO_SYNC | EP_ATTR_DATA,
        .wMaxPacketSize     = EP4_SIZE,
        .bInterval          = 1
    },

};

static const string_desc_t string_descriptor = {
    // Language code string descriptor
    {
        .bLength            = sizeof(lang_id_desc_t),
        .bDescriptorType    = USB_DESC_TYPE_STRING,
        .string             = { 0x0409 }
    },

    // Manufacturer string descriptor
    {
        .bLength            = sizeof(manuf_id_desc_t),
        .bDescriptorType    = USB_DESC_TYPE_STRING,
        .string             = { '=','E','l','v','e','e','s','=' }
    },

    // Product string descriptor
    {
        .bLength            = sizeof(prod_id_desc_t),
        .bDescriptorType    = USB_DESC_TYPE_STRING,
        .string             = { 'T','e','s','t','-','U','S','B' }
    },

    // Serial string descriptor
    {
        .bLength            = sizeof(serial_id_desc_t),
        .bDescriptorType    = USB_DESC_TYPE_STRING,
        .string             = { 'N','u','m','1' }
    },

    // Configuration ID string descriptor
    {
        .bLength            = sizeof(conf_id_desc_t),
        .bDescriptorType    = USB_DESC_TYPE_STRING,
        .string             = { 'M','A','I','N',' ','C' }
    },
};

uint8_t buf_ep1[1024] __attribute__((aligned(8)));
uint8_t buf_ep2[1024] __attribute__((aligned(8)));
uint8_t buf_ep3[1024] __attribute__((aligned(8)));
uint8_t buf_ep4[1024] __attribute__((aligned(8)));
uint8_t ep1_counter, ep2_counter, ep3_counter, ep4_counter;
unsigned ep1_bytes_in, ep3_bytes_in;
unsigned ep2_bad_data, ep4_bad_data;
unsigned ep2_bytes_out, ep4_bytes_out;
unsigned cur_int_mask = 0;
mutex_t usb_mutex, ep1_mutex, ep2_mutex, ep3_mutex, ep4_mutex;


static void hello (void *arg)
{
    unsigned prev_ep1_bytes_in = 0;
    unsigned prev_ep2_bytes_out = 0;
    unsigned prev_ep3_bytes_in = 0;
    unsigned prev_ep4_bytes_out = 0;

    debug_printf ("Free memory: %d bytes\n", mem_available (&pool));
    
    MC_USB_VENDOR_DATA = 0xabcd;

    for (;;) {
        //timer_delay (&timer, 1000);
        mdelay (1000);

        debug_printf ("EP1 in %d, rate %d | EP2 out %d, rate %d, bad %d | EP3 in %d, rate %d | EP4 out %d, rate %d, bad %d\n",
            ep1_bytes_in, ep1_bytes_in - prev_ep1_bytes_in,
            ep2_bytes_out, ep2_bytes_out - prev_ep2_bytes_out, ep2_bad_data,
            ep3_bytes_in, ep3_bytes_in - prev_ep3_bytes_in,
            ep4_bytes_out, ep4_bytes_out - prev_ep4_bytes_out, ep4_bad_data);
        prev_ep1_bytes_in = ep1_bytes_in;
        prev_ep2_bytes_out = ep2_bytes_out;
        prev_ep3_bytes_in = ep3_bytes_in;
        prev_ep4_bytes_out = ep4_bytes_out;
        
unsigned int_csr = MC_USB_INT_CSR;
    if (int_csr & MC_USB_VENDOR_SET_FEAT)
        debug_printf("Set feature request\n");
    if (int_csr & MC_USB_VENDOR_REQ)
        debug_printf("Vendor request\n");
    debug_printf("INDEX: %08X, VALUE: %08X, DATA: %08X\n",
        MC_USB_VENDOR_INDEX, MC_USB_VENDOR_VALUE, MC_USB_VENDOR_DATA);

    }
}


void reset()
{
    MC_USB_CSR = MC_USB_CLR_EP1_FIFO | MC_USB_CLR_EP2_FIFO |
        MC_USB_CLR_EP3_FIFO | MC_USB_CLR_EP4_FIFO | MC_USB_SUSPEND;

    MC_CSR_USB_EP1_RX = 0;
    MC_CSR_USB_EP2_TX = 0;
    MC_CSR_USB_EP3_RX = 0;
    MC_CSR_USB_EP4_TX = 0;

    int i;
    for (i = 0; i < sizeof(buf_ep1); ++i) {
        buf_ep1[i] = i;
        buf_ep3[i] = i;
    }

    MC_IR_USB_EP1_RX = mips_virtual_addr_to_physical((unsigned)buf_ep1);
    MC_IR_USB_EP2_TX = mips_virtual_addr_to_physical((unsigned)buf_ep2);
    MC_IR_USB_EP3_RX = mips_virtual_addr_to_physical((unsigned)buf_ep3);
    MC_IR_USB_EP4_TX = mips_virtual_addr_to_physical((unsigned)buf_ep4);

    MC_CSR_USB_EP1_RX = MC_DMA_CSR_WCX(sizeof(buf_ep1)/8 - 1) |
        MC_DMA_CSR_WN(0) | MC_DMA_CSR_RUN;
    MC_CSR_USB_EP2_TX = MC_DMA_CSR_WCX(sizeof(buf_ep2)/8 - 1) |
        MC_DMA_CSR_WN(0) | MC_DMA_CSR_RUN;
    MC_CSR_USB_EP3_RX = MC_DMA_CSR_WCX(sizeof(buf_ep3)/8 - 1) |
        MC_DMA_CSR_WN(0) | MC_DMA_CSR_RUN;
    MC_CSR_USB_EP4_TX = MC_DMA_CSR_WCX(sizeof(buf_ep4)/8 - 1) |
        MC_DMA_CSR_WN(0) | MC_DMA_CSR_RUN;

    MC_USB_CSR = MC_USB_DMA_EN;

    MC_USB_CSR_EP1 = MC_USB_EP_MAX_PACKET(EP1_SIZE) | MC_USB_EP_IN |
#if EP1_TYPE==EP_ATTR_INTR
        MC_USB_EP_INT;
#elif EP1_TYPE==EP_ATTR_BULK
        MC_USB_EP_BULK;
#else
        MC_USB_EP_ISO;
#endif

    MC_USB_CSR_EP2 = MC_USB_EP_MAX_PACKET(EP2_SIZE) | MC_USB_EP_OUT |
#if EP2_TYPE==EP_ATTR_INTR
        MC_USB_EP_INT;
#elif EP1_TYPE==EP_ATTR_BULK
        MC_USB_EP_BULK;
#else
        MC_USB_EP_ISO;
#endif

    MC_USB_CSR_EP3 = MC_USB_EP_MAX_PACKET(EP3_SIZE) | MC_USB_EP_IN |
#if EP3_TYPE==EP_ATTR_INTR
        MC_USB_EP_INT;
#elif EP1_TYPE==EP_ATTR_BULK
        MC_USB_EP_BULK;
#else
        MC_USB_EP_ISO;
#endif

    MC_USB_CSR_EP4 = MC_USB_EP_MAX_PACKET(EP4_SIZE) | MC_USB_EP_OUT |
#if EP4_TYPE==EP_ATTR_INTR
        MC_USB_EP_INT;
#elif EP1_TYPE==EP_ATTR_BULK
        MC_USB_EP_BULK;
#else
        MC_USB_EP_ISO;
#endif

    MC_USB_INT_CSR = cur_int_mask;
}

bool_t usb_int_handler(void *arg)
{
    unsigned status = MC_USB_INT_CSR;
    //status &= cur_int_mask;

    debug_printf("USB interrupt, MC_USB_INT_CSR = %08X, CURRENT MASK = %08X\n", status, cur_int_mask);
    //if (status == 0) {
    //    debug_printf("spurios interrupt, MC_USB_INT_CSR = %08X!\n", MC_USB_INT_CSR);
    //}

    if (status & MC_USB_RESET) {
        debug_printf(" ==> RESET\n");
        cur_int_mask &= ~MC_USB_RESET;
        reset();
    }
    if (status & MC_USB_CONFIGURED) {
        debug_printf(" ==> CONFIGURED\n");
        cur_int_mask &= ~MC_USB_CONFIGURED;
        MC_USB_INT_CSR = cur_int_mask;
    }
    if (status & MC_USB_ADDRESSED) {
        debug_printf(" ==> ADDRESSED\n");
        cur_int_mask &= ~MC_USB_ADDRESSED;
        MC_USB_INT_CSR = cur_int_mask;
    }
    if (status & MC_USB_BUSY) {
        debug_printf(" ==> BUSY\n");

    }
    MC_USB_INT_CSR = cur_int_mask;

    arch_intr_allow(IRQ_USB);
    return 0;
}

bool_t ep_int_handler(void *arg)
{
    int ep = (int) arg;
    int i;

    //debug_printf("EP %d interrupt\n", ep);

    switch(ep) {
    case 1:
        MC_CSR_USB_EP1_RX;
        ep1_bytes_in += sizeof(buf_ep1);
        for (i = 0; i < sizeof(buf_ep1); ++i)
            buf_ep1[i] = ep1_counter++;
        MC_IR_USB_EP1_RX = mips_virtual_addr_to_physical((unsigned)buf_ep1);
        MC_CSR_USB_EP1_RX = MC_DMA_CSR_WCX(sizeof(buf_ep1)/8 - 1) |
            MC_DMA_CSR_WN(0) | MC_DMA_CSR_RUN;
        arch_intr_allow(IRQ_USB_EP1);
        break;
    case 2:
        MC_CSR_USB_EP2_TX;
        ep2_bytes_out += sizeof(buf_ep2);
        for (i = 0; i < sizeof(buf_ep2); ++i)
            if (buf_ep2[i] != ep2_counter++) {
debug_printf("ep2 rec: %d, exp: %d\n", buf_ep2[i], ep2_counter - 1);
                ep2_bad_data++;
                ep2_counter = buf_ep2[i] + 1;

            }
        MC_IR_USB_EP2_TX = mips_virtual_addr_to_physical((unsigned)buf_ep2);
        MC_CSR_USB_EP2_TX = MC_DMA_CSR_WCX(sizeof(buf_ep2)/8 - 1) |
            MC_DMA_CSR_WN(0) | MC_DMA_CSR_RUN;

        arch_intr_allow(IRQ_USB_EP2);
    case 3:
        MC_CSR_USB_EP3_RX;
        ep3_bytes_in += sizeof(buf_ep3);
        for (i = 0; i < sizeof(buf_ep3); ++i)
            buf_ep3[i] = ep3_counter++;
        MC_IR_USB_EP3_RX = mips_virtual_addr_to_physical((unsigned)buf_ep3);
        MC_CSR_USB_EP3_RX = MC_DMA_CSR_WCX(sizeof(buf_ep3)/8 - 1) |
            MC_DMA_CSR_WN(0) | MC_DMA_CSR_RUN;
        arch_intr_allow(IRQ_USB_EP3);
        break;
    case 4:
        MC_CSR_USB_EP4_TX;
        ep4_bytes_out += sizeof(buf_ep4);
        for (i = 0; i < sizeof(buf_ep4); ++i)
            if (buf_ep4[i] != ep4_counter++) {
debug_printf("ep4 rec: %d, exp: %d, i = %d\n", buf_ep4[i], ep4_counter - 1, i);
                ep4_bad_data++;
                ep4_counter = buf_ep4[i] + 1;

            }
        MC_IR_USB_EP4_TX = mips_virtual_addr_to_physical((unsigned)buf_ep4);
        MC_CSR_USB_EP4_TX = MC_DMA_CSR_WCX(sizeof(buf_ep4)/8 - 1) |
            MC_DMA_CSR_WN(0) | MC_DMA_CSR_RUN;

        arch_intr_allow(IRQ_USB_EP4);
    }

    return 0;
}

void uos_init (void)
{
    int i;
    debug_printf("\n\nTesting USBIC\n");

    MC_CLKEN |= MC_CLKEN_USB;
    udelay(1);

    MC_USB_CSR = MC_USB_CLR_EP1_FIFO | MC_USB_CLR_EP2_FIFO |
        MC_USB_CLR_EP3_FIFO | MC_USB_CLR_EP4_FIFO | MC_USB_SUSPEND;
    udelay(1);
    MC_USB_CSR = 0;

    debug_printf("USBIC REV: %08X\n", MC_USB_REVISION);
    debug_printf("Size of config descriptor: %d\n", sizeof(config_descriptor));
    debug_printf("Size of string descriptor: %d\n", sizeof(string_descriptor));

    uint8_t *p = (uint8_t *)&device_descriptor;
    for (i = 0; i < sizeof(device_descriptor); i++)
    {
        MC_USB_CFG_ADDR = i;
        MC_USB_CFG_DATA = p[i];
    }
    p = (uint8_t *)&config_descriptor;
    for (i = 0; i < sizeof(config_descriptor); i++)
    {
        MC_USB_CFG_ADDR = sizeof(device_descriptor) + i;
        MC_USB_CFG_DATA = p[i];
    }

    p = (uint8_t *)&string_descriptor;
    for (i = 0; i < sizeof(string_descriptor); i++)
    {
        MC_USB_CFG_ADDR = sizeof(device_descriptor) + sizeof(config_descriptor) + i;
        MC_USB_CFG_DATA = p[i];
        debug_printf("%02X ", p[i]);
    }

    for (i = 0; i < 128 - (sizeof(device_descriptor) + sizeof(config_descriptor) + sizeof(string_descriptor)); ++i) {
        MC_USB_CFG_ADDR = sizeof(device_descriptor) + sizeof(config_descriptor) + sizeof(string_descriptor) + i;
        MC_USB_CFG_DATA = 0;
    }

    debug_printf("\n");

    cur_int_mask = MC_USB_CONFIGURED | MC_USB_ADDRESSED /*| MC_USB_BUSY*/ | MC_USB_RESET;
    reset();
    
//#define CHECK_VENDOR_REQ
#ifdef CHECK_VENDOR_REQ
    unsigned int_csr = MC_USB_INT_CSR;
    while (! (MC_USB_INT_CSR & (MC_USB_VENDOR_SET_FEAT | MC_USB_VENDOR_REQ)))
        int_csr = MC_USB_INT_CSR;
    
    if (int_csr & MC_USB_VENDOR_SET_FEAT)
        debug_printf("Set feature request\n");
    if (int_csr & MC_USB_VENDOR_REQ)
        debug_printf("Vendor request\n");
    debug_printf("INDEX: %08X, VALUE: %08X, DATA: %08X\n",
        MC_USB_VENDOR_INDEX, MC_USB_VENDOR_VALUE, MC_USB_VENDOR_DATA);
#endif

    //mutex_attach_irq (&usb_mutex, IRQ_USB, (handler_t) usb_int_handler, 0);
    mutex_attach_irq (&ep1_mutex, IRQ_USB_EP1, (handler_t) ep_int_handler, (void*)1);
    mutex_attach_irq (&ep2_mutex, IRQ_USB_EP2, (handler_t) ep_int_handler, (void*)2);
    mutex_attach_irq (&ep3_mutex, IRQ_USB_EP3, (handler_t) ep_int_handler, (void*)3);
    mutex_attach_irq (&ep4_mutex, IRQ_USB_EP4, (handler_t) ep_int_handler, (void*)4);

    task_create (hello, 0, "hello", 1, task, sizeof (task));
}
