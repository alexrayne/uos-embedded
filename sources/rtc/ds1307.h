#ifndef __DS1307_H__
#define __DS1307_H__

#include <rtc/rtc-interface.h>
#include <i2c/i2c-interface.h>

typedef struct _ds1307_t ds1307_t;
struct _ds1307_t
{
    rtcif_t         rtcif;
    
    i2cif_t         *i2c;
    i2c_message_t   msg;
};

int ds1307_init(ds1307_t *ds1307, i2cif_t *i2c);

#endif // __DS1307_H__
