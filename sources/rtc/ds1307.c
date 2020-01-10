#include <runtime/lib.h>
#include <kernel/uos.h>
#include "ds1307.h"

#define GET_SU(x)       ((x) & 0xF)
#define GET_ST(x)       (((x) >> 4) & 0x7)
#define GET_MNU(x)      ((x) & 0xF)
#define GET_MNT(x)      (((x) >> 4) & 0x7)
#define GET_HU(x)       ((x) & 0xF)
#define GET_HT(x)       (((x) >> 4) & 0x3)
#define GET_DU(x)       ((x) & 0xF)
#define GET_DT(x)       (((x) >> 4) & 0x3)
#define GET_MU(x)       ((x) & 0xF)
#define GET_MT(x)       (((x) >> 4) & 0x1)
#define GET_YU(x)       ((x) & 0xF)
#define GET_YT(x)       (((x) >> 4) & 0xF)

#define SET_UNITS(x)    ((x) & 0xF)
#define SET_TENS(x)     (((x) & 0xF) << 4)

enum {
    REG_SECONDS,
    REG_MINUTES,
    REG_HOURS,
    REG_DAY,
    REG_DATE,
    REG_MONTH,
    REG_YEAR,
    REG_CONTROL
};

static int ds1307_get_time(rtcif_t *rtcif, datetime_t *dt)
{
    ds1307_t *ds1307 = (ds1307_t *) rtcif;
    i2c_transaction_t set_address;
    i2c_transaction_t read_data;
    uint8_t address_byte = 0;
    uint8_t date_data[7];
    
    memset(date_data, 0, sizeof(date_data));

    ds1307->msg.first = &set_address;
        
    set_address.data = &address_byte;
    set_address.size = I2C_TX_SIZE(1);
    set_address.next = &read_data;
    
    read_data.data = date_data;
    read_data.size = I2C_RX_SIZE(sizeof(date_data));
    read_data.next = 0;
    
    if (i2c_trx(ds1307->i2c, &ds1307->msg) != I2C_ERR_OK)
        return RTC_ERR_IO;

    dt->year = 2000 +   GET_YT(date_data[REG_YEAR])     * 10 + GET_YU(date_data[REG_YEAR]);
    dt->month =         GET_MT(date_data[REG_MONTH])    * 10 + GET_MU(date_data[REG_MONTH]);
    dt->day =           GET_DT(date_data[REG_DATE])     * 10 + GET_DU(date_data[REG_DATE]);
    dt->hour =          GET_HT(date_data[REG_HOURS])    * 10 + GET_HU(date_data[REG_HOURS]);
    dt->minute =        GET_MNT(date_data[REG_MINUTES]) * 10 + GET_MNU(date_data[REG_MINUTES]);
    dt->second =        GET_ST(date_data[REG_SECONDS])  * 10 + GET_SU(date_data[REG_SECONDS]);
    dt->nsecond = 0;
    
    return RTC_ERR_OK;
}

static int ds1307_set_time(rtcif_t *rtcif, const datetime_t *dt)
{
    /*
    uint32_t ssr = 65536ull * dt->nsecond / 1000000000ull;
    uint32_t tr  = RTC_ST(dt->second / 10) | RTC_SU(dt->second % 10) |
        RTC_MNT(dt->minute / 10) | RTC_MNU(dt->minute % 10) |
        RTC_HT(dt->hour / 10) | RTC_HU(dt->hour % 10);
    uint32_t dr  = RTC_DT(dt->day / 10) | RTC_DU(dt->day % 10) |
        RTC_MT(dt->month / 10) | RTC_MU(dt->month % 10) |
        RTC_YT((dt->year - 2000) / 10) | RTC_YU((dt->year - 2000) % 10);
        */
        
    ds1307_t *ds1307 = (ds1307_t *) rtcif;
    i2c_transaction_t set_date;
    uint8_t tx_buffer[8];
    uint8_t *date_data = tx_buffer + 1;
    
    tx_buffer[0] = 0;   // Начальный адрес
    date_data[REG_SECONDS] = SET_TENS(dt->second / 10) | SET_UNITS(dt->second % 10);
    date_data[REG_MINUTES] = SET_TENS(dt->minute / 10) | SET_UNITS(dt->minute % 10);
    date_data[REG_HOURS]   = SET_TENS(dt->hour / 10)   | SET_UNITS(dt->hour % 10);
    date_data[REG_DATE]    = SET_TENS(dt->day / 10)    | SET_UNITS(dt->day % 10);
    date_data[REG_MONTH]   = SET_TENS(dt->month / 10)  | SET_UNITS(dt->month % 10);
    date_data[REG_YEAR]    = SET_TENS((dt->year - 2000) / 10) |
        SET_UNITS((dt->year - 2000) % 10);
    
    ds1307->msg.first = &set_date;
    set_date.data = tx_buffer;
    set_date.size = I2C_TX_SIZE(8);
    set_date.next = 0;
    
    if (i2c_trx(ds1307->i2c, &ds1307->msg) != I2C_ERR_OK)
        return RTC_ERR_IO;
        
    return RTC_ERR_OK;
}

int ds1307_init(ds1307_t *ds1307, i2cif_t *i2c)
{
	ds1307->rtcif.get_time = ds1307_get_time;
	ds1307->rtcif.set_time = ds1307_set_time;
	
	ds1307->msg.mode = I2C_MODE_SLAVE_ADDR(0xD0) | I2C_MODE_7BIT_ADDR |
	    I2C_MODE_FM | I2C_MODE_FREQ_KHZ(100);
	ds1307->i2c = i2c;
	
	return RTC_ERR_OK;
}
