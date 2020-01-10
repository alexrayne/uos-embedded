#ifndef __S25FL_K_H__
#define __S25FL_K_H__

#include <flash/flash-interface.h>
#include <spi/spi-master-interface.h>

struct _s25fl_k_t
{
    flashif_t       flashif;
    spimif_t       *spi;
    spi_message_t   msg;
    uint8_t         databuf[20];
};
typedef struct _s25fl_k_t s25fl_k_t;

void s25fl_k_init(s25fl_k_t *m, spimif_t *s, unsigned freq, unsigned mode);

#endif
