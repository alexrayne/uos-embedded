#ifndef __DS7505_H__
#define __DS7505_H__

#include <thermo/thermo-interface.h>
#include <i2c/i2c-interface.h>


// ВСЕ ТЕМПЕРАТУРЫ В ПАРАМЕТРАХ ФУНКЦИЙ ЗАДАЮТСЯ И ВОЗВРАЩАЮТСЯ В ФОРМАТЕ (ЗНАЧЕНИЕ * 1000)!


typedef struct _ds7505_t
{
    thermoif_t      thermoif;
    
    i2cif_t         *i2c;
    i2c_message_t   msg;
} ds7505_t;


// Функция инициализации драйвера DS7505. Можно вызывать из uos_init().
// i2c          - указатель на драйвер шины i2c.
// i2c_addr     - адрес микросхемы на шине i2c (старшие 7 битов).
// i2c_timings  - режим времянки (таймингов) шины i2c; возможные значения:
//                      I2C_MODE_SM     - времянка Sm (Standard-mode);
//                      I2C_MODE_SMBUS  - времянка SMBUS;
//                      I2C_MODE_FM     - времянка Fm (Fast-mode).
// i2c_freq_khz - бодовая частота передачи на шине i2c в кГц.
void ds7505_init(ds7505_t *ds7505, i2cif_t *i2c, uint8_t i2c_addr, 
    unsigned i2c_timings, unsigned i2c_freq_khz);

//
// Установка параметров DS7505
//

// Макроопределения для параметра mode функции ds7505_set_params
// (в соответствии с Table 2 спецификации версии 3).
// 1) Выключение, если бит установлен
#define DS7505_MODE_SHUTDOWN        (1 << 0)
// 2) Установка режима термостата (бит TM).
#define DS7505_MODE_COMPARATOR      (0 << 1)    // компаратор
#define DS7505_MODE_INTERRUPT       (1 << 1)    // прерывания
// 3) Полярность сигнала O.S. (бит POL).
#define DS7505_OS_ACTIVE_LOW        (0 << 2)    // активный низкий
#define DS7505_OS_ACTIVE_HIGH       (1 << 2)    // активный высокий
// 4) Количество чтений вне текущего диапазона перед переключением O.S. (биты F0, F1).
#define DS7505_OS_DEBOUNCE_1        (0 << 3)    // 1 чтение
#define DS7505_OS_DEBOUNCE_2        (1 << 3)    // 2 чтения
#define DS7505_OS_DEBOUNCE_4        (2 << 3)    // 4 чтения
#define DS7505_OS_DEBOUNCE_6        (3 << 3)    // 6 чтений
// 5) Количество битов в результате преобразования АЦП (биты R0, R1).
#define DS7505_RESOLUTION_9BITS     (0 << 5)    // 9 битов
#define DS7505_RESOLUTION_10BITS    (1 << 5)    // 10 битов
#define DS7505_RESOLUTION_11BITS    (2 << 5)    // 11 битов
#define DS7505_RESOLUTION_12BITS    (3 << 5)    // 12 битов

//
// Функция установки параметров DS7505.
// mode     - режим (битовое поле, см. макросы выше).
// Возвращает код завершения в соответствии с thermo-interface.h.
//
int ds7505_set_mode(ds7505_t *ds7505, uint8_t mode);

//
// Функция установки температур термостата.
// t_os     - при превышении данной температуры выход O.S. примет активный
//            уровень; температура задаётся в градусах Цельсия, умноженных на 1000.
// t_hyst   - при снижении температуры ниже данной выход O.S. примет пассивный
//            уровень; температура задаётся в градусах Цельсия, умноженных на 1000.
// Возвращает код завершения в соответствии с thermo-interface.h.
//
int ds7505_set_thermostat(ds7505_t *ds7505, int t_os, int t_hyst);

// Чтение установки Tos. В случае ошибки возвращается значение (0x7FFFFFFF + код ошибки).
int ds7505_read_t_os(ds7505_t *ds7505);

// Чтение установки Thyst. В случае ошибки возвращается значение (0x7FFFFFFF + код ошибки).
int ds7505_read_t_hyst(ds7505_t *ds7505);

//
// Функция записи установок в EEPROM микросхемы.
// Возвращает код завершения в соответствии с thermo-interface.h.
//
int ds7505_burn_settings(ds7505_t *ds7505);

#endif // __DS7505_H__
