#include <runtime/lib.h>
#include <kernel/uos.h>
#include <thermo/ds7505.h>

static int ds7505_read(thermoif_t *thermo)
{
    ds7505_t *ds7505 = (ds7505_t *) thermo;
    i2c_transaction_t trans1;       // Переключение внутреннего указателя на регистр температуры
    i2c_transaction_t trans2;       // Чтение текущей температуры
    uint8_t ds_pointer_to_temp  = 0;
    uint8_t ds_temp[2];
    
    trans1.data = &ds_pointer_to_temp;
    trans1.size = I2C_TX_SIZE(1);
    trans1.next = &trans2;
    trans2.data = ds_temp;
    trans2.size = I2C_RX_SIZE(2);
    trans2.next = 0;
    
    ds7505->msg.first = &trans1;

    int res = i2c_trx(ds7505->i2c, &ds7505->msg);
    if (res != I2C_ERR_OK)
        return 0x7FFFFFFF + THERMO_ERR_IO;
     
    short res_temp = (ds_temp[0] << 8) | ds_temp[1];
    return res_temp * 1000 / 256;
}

int ds7505_set_mode(ds7505_t *ds7505, uint8_t mode)
{
    i2c_transaction_t trans1;       // Запись регистра конфигурации
    uint8_t conf[2];
    
    conf[0] = 1;    // Переключение внутреннего указателя на регистр конфигурации
    conf[1] = mode;
    
    trans1.data = conf;
    trans1.size = I2C_TX_SIZE(2);
    trans1.next = 0;

    ds7505->msg.first = &trans1;

    int res = i2c_trx(ds7505->i2c, &ds7505->msg);
    if (res != I2C_ERR_OK)
        return THERMO_ERR_IO;
    
    return THERMO_ERR_OK;
}

int ds7505_set_thermostat(ds7505_t *ds7505, int t_os, int t_hyst)
{
    i2c_transaction_t trans1;       // Запись Thyst
    i2c_transaction_t trans2;       // Запись Tos
    uint8_t ds_pointer_to_thyst = 2;
    uint8_t ds_pointer_to_tos   = 3;
    
    uint8_t t_hyst_data[3];
    t_hyst_data[0] = ds_pointer_to_thyst;
    t_hyst_data[1] = t_hyst / 1000;
    t_hyst_data[2] = t_hyst * 256 / 1000;

    uint8_t t_os_data[3];
    t_os_data[0] = ds_pointer_to_tos;
    t_os_data[1] = t_os / 1000;
    t_os_data[2] = t_os * 256 / 1000;
    
    trans1.data = t_hyst_data;
    trans1.size = I2C_TX_SIZE(3);
    trans1.next = &trans2;
    trans2.data = t_os_data;
    trans2.size = I2C_TX_SIZE(3);
    trans2.next = 0;
    
    ds7505->msg.first = &trans1;
    
    int res = i2c_trx(ds7505->i2c, &ds7505->msg);
    if (res != I2C_ERR_OK)
        return THERMO_ERR_IO;
        
    return THERMO_ERR_OK;    
}

int ds7505_read_t_os(ds7505_t *ds7505)
{
    i2c_transaction_t trans1;       // Переключение внутреннего указателя на Tos
    i2c_transaction_t trans2;       // Чтение Tos
    uint8_t ds_pointer_to_tos   = 3;
    uint8_t ds_temp[2];

    trans1.data = &ds_pointer_to_tos;
    trans1.size = I2C_TX_SIZE(1);
    trans1.next = &trans2;
    trans2.data = ds_temp;
    trans2.size = I2C_RX_SIZE(2);
    trans2.next = 0;
    
    ds7505->msg.first = &trans1;
    
    int res = i2c_trx(ds7505->i2c, &ds7505->msg);
    if (res != I2C_ERR_OK)
        return 0x7FFFFFFF + THERMO_ERR_IO;
     
    short res_temp = (ds_temp[0] << 8) | ds_temp[1];
    return res_temp * 1000 / 256;
}

int ds7505_read_t_hyst(ds7505_t *ds7505)
{
    i2c_transaction_t trans1;       // Переключение внутреннего указателя на Thyst
    i2c_transaction_t trans2;       // Чтение Thyst
    uint8_t ds_pointer_to_thyst = 2;
    uint8_t ds_temp[2];

    trans1.data = &ds_pointer_to_thyst;
    trans1.size = I2C_TX_SIZE(1);
    trans1.next = &trans2;
    trans2.data = ds_temp;
    trans2.size = I2C_RX_SIZE(2);
    trans2.next = 0;
    
    ds7505->msg.first = &trans1;
    
    int res = i2c_trx(ds7505->i2c, &ds7505->msg);
    if (res != I2C_ERR_OK)
        return 0x7FFFFFFF + THERMO_ERR_IO;
     
    short res_temp = (ds_temp[0] << 8) | ds_temp[1];
    return res_temp * 1000 / 256;
}

int ds7505_burn_settings(ds7505_t *ds7505)
{
    i2c_transaction_t trans1;           // Команда на запись настроек в EEPROM микросхемы
    uint8_t ds_copy_data_cmd = 0x48;
    
    trans1.data = &ds_copy_data_cmd;
    trans1.size = I2C_TX_SIZE(1);
    trans1.next = 0;
    
    ds7505->msg.first = &trans1;

    int res = i2c_trx(ds7505->i2c, &ds7505->msg);
    if (res != I2C_ERR_OK)
        return THERMO_ERR_IO;

    return THERMO_ERR_OK;
}

void ds7505_init(ds7505_t *ds7505, i2cif_t *i2c, uint8_t i2c_addr, unsigned i2c_timings, unsigned i2c_freq_khz)
{
    ds7505->i2c = i2c;
    ds7505->msg.mode = I2C_MODE_SLAVE_ADDR(i2c_addr) | I2C_MODE_7BIT_ADDR | 
        i2c_timings | I2C_MODE_FREQ_KHZ(i2c_freq_khz);
    
    ds7505->thermoif.read = ds7505_read;
}
