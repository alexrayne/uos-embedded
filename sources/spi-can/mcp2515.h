#ifndef __MCP2515_H__
#define __MCP2515_H__

#include <can/can-interface.h>
#include <spi/spi-master-interface.h>

typedef struct _mcp2515_t
{
    canif_t             canif;
    
    int                 cur_mode;
    unsigned            osc_hz;
    unsigned            timing;
    unsigned            baud_rate;
    
    spimif_t            *spi;
    spi_message_t       msg;
    uint8_t             dataout[16];
    uint8_t             datain[16];
    
    uint8_t             int_flags;
} mcp2515_t;

typedef struct _mcp2515_frame_t
{
    uint8_t     sidh;
    uint8_t     sidl;
    uint8_t     eid8;
    uint8_t     eid0;
    uint8_t     dlc;
    uint8_t     data[8];
} __attribute((packed)) mcp2515_frame_t;

typedef enum {
    MCP2515_PIN_RX0BF,
    MCP2515_PIN_RX1BF,
    MCP2515_PIN_TX0RTS,
    MCP2515_PIN_TX1RTS,
    MCP2515_PIN_TX2RTS,
    MCP2515_NB_OF_PINS
} mcp2515_pin_t;

int mcp2515_init(mcp2515_t *mcp2515, spimif_t *spi, int spi_cs_num, unsigned spi_freq_hz, unsigned osc_hz);
int mcp2515_enable_gpio(mcp2515_t *mcp2515, mcp2515_pin_t pin, int set_value);
int mcp2515_disable_gpio(mcp2515_t *mcp2515, mcp2515_pin_t pin);
int mcp2515_gpio_set_value(mcp2515_t *mcp2515, mcp2515_pin_t pin, int value);
int mcp2515_gpio_get_value(mcp2515_t *mcp2515, mcp2515_pin_t pin);

#endif // __MCP2515_H__
