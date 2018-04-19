#include <runtime/lib.h>
#include <kernel/uos.h>
#include <thermo/adt7461.h>

#define RD_LOC_TEMP             0x00
#define RD_EXT_TEMP_HI          0x01
#define RD_STATUS               0x02
#define RD_CONF                 0x03
#define RD_CONV_RATE            0x04
#define RD_LOCAL_TEMP_HI_LIM    0x05
#define RD_LOCAL_TEMP_LO_LIM    0x06
#define RD_EXT_TEMP_HI_LIM_HI   0x07
#define RD_EXT_TEMP_LO_LIM_HI   0x08
#define RD_EXT_TEMP_LO          0x10
#define RD_EXT_TEMP_OFFSET_HI   0x11
#define RD_EXT_TEMP_OFFSET_LO   0x12
#define RD_EXT_TEMP_HI_LIM_LO   0x13
#define RD_EXT_TEMP_LO_LIM_LO   0x14
#define RD_EXT_THERM_LIM        0x19
#define RD_LOC_THERM_LIM        0x20
#define RD_THERM_HYST           0x21
#define RD_CONS_ALERT           0x22
#define RD_MANUF_ID             0xFE
#define RD_DIE_REV_CODE         0xFF

#define WR_CONF                 0x09
#define WR_CONV_RATE            0x0A
#define WR_LOC_TEMP_HI_LIM      0x0B
#define WR_LOC_TEMP_LO_LIM      0x0C
#define WR_EXT_TEMP_HI_LIM_HI   0x0D
#define WR_EXT_TEMP_LO_LIM_HI   0x0E
#define WR_ONE_SHOT             0x0F
#define WR_EXT_TEMP_OFFSET_HI   0x11
#define WR_EXT_TEMP_OFFSET_LO   0x12
#define WR_EXT_TEMP_HI_LIM_LO   0x13
#define WR_EXT_TEMP_LO_LIM_LO   0x14
#define WR_EXT_THERM_LIM        0x19
#define WR_LOC_THERM_LIM        0x20
#define WR_THERM_HYST           0x21
#define WR_CONS_ALERT           0x22

#define MANUFACTURER_ID         0x41
#define DIE_REVISION_CODE       0x57

#define CONF_MASK1              (1 << 7)
#define CONF_STOP               (1 << 6)
#define CONF_THERM2             (1 << 5)
#define CONF_EXTENDED_RANGE     (1 << 2)

#define ALERT_NB_DEBOUNCE(x)    (((x) - 1) << 1)
#define ALERT_SMBUS_TIMEOUT     (1 << 7)

static int adt7461_read_local_temp(thermoif_t *thermo)
{
    adt7461_t *adt7461 = container_of(thermo, adt7461_t, local_temp_sensor);
    i2c_transaction_t trans1;       // Переключение внутреннего указателя на регистр температуры
    i2c_transaction_t trans2;       // Чтение текущей температуры
    uint8_t addr;
    uint8_t temp;
    int res;

    trans1.data = &addr;
    trans1.size = I2C_TX_SIZE(1);
    trans1.next = &trans2;
    trans2.data = &temp;
    trans2.next = 0;
    
    adt7461->msg.first = &trans1;
    
    if (adt7461->standby) {
        // Если микросхема находится в режими Standby, то делаем запись произвольного значения
        // в регистр One-Shot, что выполнить чтение температуры по запросу
        trans2.size = I2C_TX_SIZE(1);
        addr = WR_ONE_SHOT;
        
        res = i2c_trx(adt7461->i2c, &adt7461->msg);
        if (res != I2C_ERR_OK)
            return 0x7FFFFFFF + THERMO_ERR_IO;
    }
    
    addr = RD_LOC_TEMP;
    trans2.size = I2C_RX_SIZE(1);

    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return 0x7FFFFFFF + THERMO_ERR_IO;
     
    return (temp - 64) * 1000;
}

static int adt7461_read_external_temp(thermoif_t *thermo)
{
    adt7461_t *adt7461 = container_of(thermo, adt7461_t, external_temp_sensor);
    i2c_transaction_t trans1;       // Переключение внутреннего указателя на байт внешней темп-ры
    i2c_transaction_t trans2;       // Чтение байта внешней темп-ры
    uint8_t addr;
    uint16_t temp;
    int res;
    
    trans1.data = &addr;
    trans1.size = I2C_TX_SIZE(1);
    trans1.next = &trans2;
    trans2.data = &temp;
    trans2.next = 0;

    adt7461->msg.first = &trans1;
    
    if (adt7461->standby) {
        // Если микросхема находится в режими Standby, то делаем запись произвольного значения
        // в регистр One-Shot, что выполнить чтение температуры по запросу
        trans2.size = I2C_TX_SIZE(1);
        addr = WR_ONE_SHOT;

        res = i2c_trx(adt7461->i2c, &adt7461->msg);
        if (res != I2C_ERR_OK)
            return 0x7FFFFFFF + THERMO_ERR_IO;
    }

    addr = RD_EXT_TEMP_LO;    
    trans2.size = I2C_RX_SIZE(1);

    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return 0x7FFFFFFF + THERMO_ERR_IO;

    addr = RD_EXT_TEMP_HI;
    trans2.data = (uint8_t *)&temp + 1;

    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return 0x7FFFFFFF + THERMO_ERR_IO;

    return temp * 1000 / 256 - 64000;
}

int adt7461_set_conversion_period(adt7461_t *adt7461, uint8_t enum_conv_period)
{
    if (enum_conv_period > THERMO_ERR_BAD_PARAM)
        return THERMO_ERR_BAD_PARAM;

    i2c_transaction_t trans;
    uint8_t wr_data[2] = {WR_CONV_RATE, enum_conv_period};
    int res;
    
    trans.data = wr_data;
    trans.size = I2C_TX_SIZE(2);
    trans.next = 0;
    adt7461->msg.first = &trans;
    
    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return 0x7FFFFFFF + THERMO_ERR_IO;
        
    return THERMO_ERR_OK;
}

int adt7461_set_alert_mode(adt7461_t *adt7461, int pin_mode, int nb_debounce)
{
    i2c_transaction_t trans;
    uint8_t wr_data[2];
    int res;
    
    wr_data[0] = WR_CONF;
    wr_data[1] = CONF_EXTENDED_RANGE | pin_mode |
        ((adt7461->standby) ? CONF_STOP : 0);

    trans.data = wr_data;
    trans.size = I2C_TX_SIZE(2);
    trans.next = 0;
    adt7461->msg.first = &trans;
    
    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return 0x7FFFFFFF + THERMO_ERR_IO;

    wr_data[0] = WR_CONS_ALERT;
    wr_data[1] = ALERT_NB_DEBOUNCE(nb_debounce) | ALERT_SMBUS_TIMEOUT;

    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return 0x7FFFFFFF + THERMO_ERR_IO;
        
    return THERMO_ERR_OK;
}

int adt7461_set_therm_hysteresis(adt7461_t *adt7461, int therm_hyst)
{
    i2c_transaction_t trans;
    uint8_t wr_data[2] = {WR_THERM_HYST, therm_hyst / 1000};
    int res;
    
    trans.data = wr_data;
    trans.size = I2C_TX_SIZE(2);
    trans.next = 0;
    adt7461->msg.first = &trans;
    
    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return 0x7FFFFFFF + THERMO_ERR_IO;
        
    return THERMO_ERR_OK;
}

int adt7461_set_local_thermostat(adt7461_t *adt7461, int low_limit, int high_limit, int therm_limit)
{
    i2c_transaction_t trans;
    uint8_t wr_data[2];
    int res;
    
    trans.data = wr_data;
    trans.size = I2C_TX_SIZE(2);
    trans.next = 0;
    adt7461->msg.first = &trans;

    wr_data[0] = WR_LOC_TEMP_LO_LIM;
    wr_data[1] = low_limit / 1000 + 64;
    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return THERMO_ERR_IO;

    wr_data[0] = WR_LOC_TEMP_HI_LIM;
    wr_data[1] = high_limit / 1000 + 64;
    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return THERMO_ERR_IO;

    wr_data[0] = WR_LOC_THERM_LIM;
    wr_data[1] = therm_limit / 1000 + 64;
    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return THERMO_ERR_IO;

    return THERMO_ERR_OK;
}

int adt7461_set_external_thermostat(adt7461_t *adt7461, int low_limit, int high_limit, int therm_limit)
{
    i2c_transaction_t trans;
    uint8_t wr_data[2];
    uint16_t adt_limit;
    int res;
    
    trans.data = wr_data;
    trans.size = I2C_TX_SIZE(2);
    trans.next = 0;
    adt7461->msg.first = &trans;
    
    adt_limit = (low_limit + 64000) * 256 / 1000;

    wr_data[0] = WR_EXT_TEMP_LO_LIM_LO;
    wr_data[1] = adt_limit & 0xC0;
    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return THERMO_ERR_IO;

    wr_data[0] = WR_EXT_TEMP_LO_LIM_HI;
    wr_data[1] = ((uint8_t *)&adt_limit)[1];
    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return THERMO_ERR_IO;

    adt_limit = (high_limit + 64000) * 256 / 1000;

    wr_data[0] = WR_EXT_TEMP_HI_LIM_LO;
    wr_data[1] = adt_limit & 0xC0;
    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return THERMO_ERR_IO;

    wr_data[0] = WR_EXT_TEMP_HI_LIM_HI;
    wr_data[1] = ((uint8_t *)&adt_limit)[1];
    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return THERMO_ERR_IO;

    wr_data[0] = WR_EXT_THERM_LIM;
    wr_data[1] = therm_limit / 1000 + 64;
    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return THERMO_ERR_IO;

    return THERMO_ERR_OK;
}

int adt7461_set_external_offset(adt7461_t *adt7461, int ext_offset)
{
    i2c_transaction_t trans;
    uint8_t wr_data[2];
    uint16_t adt_offset = ext_offset * 256 / 1000;
    int res;
    
    trans.data = wr_data;
    trans.size = I2C_TX_SIZE(2);
    trans.next = 0;
    adt7461->msg.first = &trans;

    wr_data[0] = WR_EXT_TEMP_OFFSET_LO;
    wr_data[1] = adt_offset & 0xC0;
    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return THERMO_ERR_IO;

    wr_data[0] = WR_EXT_TEMP_OFFSET_HI;
    wr_data[1] = ((uint8_t *)&adt_offset)[1];
    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return THERMO_ERR_IO;
    
    return THERMO_ERR_OK;
}

int adt7461_standby(adt7461_t *adt7461)
{
    i2c_transaction_t trans1;
    i2c_transaction_t trans2;
    uint8_t addr = RD_CONF;
    uint8_t conf_value;
    int res;
    
    trans1.data = &addr;
    trans1.size = I2C_TX_SIZE(1);
    trans1.next = &trans2;
    trans2.data = &conf_value;
    trans2.size = I2C_RX_SIZE(1);
    trans2.next = 0;

    adt7461->msg.first = &trans1;

    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return 0x7FFFFFFF + THERMO_ERR_IO;
        
    conf_value |= CONF_STOP;
    addr = WR_CONF;
    trans2.size = I2C_TX_SIZE(1);
    
    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return 0x7FFFFFFF + THERMO_ERR_IO;
        
    adt7461->standby = 1;

    return THERMO_ERR_OK;    
}

int adt7461_wakeup(adt7461_t *adt7461)
{
    i2c_transaction_t trans1;
    i2c_transaction_t trans2;
    uint8_t addr = RD_CONF;
    uint8_t conf_value;
    int res;
    
    trans1.data = &addr;
    trans1.size = I2C_TX_SIZE(1);
    trans1.next = &trans2;
    trans2.data = &conf_value;
    trans2.size = I2C_RX_SIZE(1);
    trans2.next = 0;

    adt7461->msg.first = &trans1;

    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return 0x7FFFFFFF + THERMO_ERR_IO;
        
    conf_value &= ~CONF_STOP;
    addr = WR_CONF;
    trans2.size = I2C_TX_SIZE(1);
    
    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return 0x7FFFFFFF + THERMO_ERR_IO;

    adt7461->standby = 0;
    
    return THERMO_ERR_OK;    
}

int adt7461_init(adt7461_t *adt7461, i2cif_t *i2c, uint8_t i2c_addr, 
        unsigned i2c_freq_khz, int standby)
{
    adt7461->i2c = i2c;
    adt7461->msg.mode = I2C_MODE_SLAVE_ADDR(i2c_addr) | I2C_MODE_7BIT_ADDR | 
        I2C_MODE_SMBUS | I2C_MODE_FREQ_KHZ(i2c_freq_khz);
        
    i2c_transaction_t trans;
    uint8_t wr_data[2];
    int res;
    
    trans.data = wr_data;
    trans.size = I2C_TX_SIZE(2);
    trans.next = 0;
    adt7461->msg.first = &trans;

    // Включение расширенного диапазона температур.
    // Выполняем действия по перепрограммированию лимитов температур
    // в выключенном режиме, что не вызвать ложных тревожных сигналов
    // или включений термостата.
    wr_data[0] = WR_CONF;
    wr_data[1] = CONF_STOP | CONF_EXTENDED_RANGE;
    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return THERMO_ERR_IO;

    // Перепрограммирование лимитов
    wr_data[0] = WR_LOC_TEMP_HI_LIM;
    wr_data[1] = 0xFF;
    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return THERMO_ERR_IO;

    wr_data[0] = WR_EXT_TEMP_HI_LIM_HI;
    wr_data[1] = 0xFF;
    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return THERMO_ERR_IO;

    wr_data[0] = WR_EXT_THERM_LIM;
    wr_data[1] = 0xFF;
    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return THERMO_ERR_IO;

    wr_data[0] = WR_LOC_THERM_LIM;
    wr_data[1] = 0xFF;
    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return THERMO_ERR_IO;

    // Включение тайм-аута SMBus        
    wr_data[0] = WR_CONS_ALERT;
    wr_data[1] = ALERT_SMBUS_TIMEOUT;
    res = i2c_trx(adt7461->i2c, &adt7461->msg);
    if (res != I2C_ERR_OK)
        return THERMO_ERR_IO;
        
    if (!standby) {
        // Возвращаем микросхему в активный режим
        wr_data[0] = WR_CONF;
        wr_data[1] = CONF_EXTENDED_RANGE;
        res = i2c_trx(adt7461->i2c, &adt7461->msg);
        if (res != I2C_ERR_OK)
            return THERMO_ERR_IO;
    }
    
    adt7461->standby = standby;
    
    adt7461->local_temp_sensor.read = adt7461_read_local_temp;
    adt7461->external_temp_sensor.read = adt7461_read_external_temp;
    
    return THERMO_ERR_OK;
}
