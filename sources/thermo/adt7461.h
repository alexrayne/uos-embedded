#ifndef __ADT7461_H__
#define __ADT7461_H__

#include <thermo/thermo-interface.h>
#include <i2c/i2c-interface.h>


// ВСЕ ТЕМПЕРАТУРЫ В ПАРАМЕТРАХ ФУНКЦИЙ ЗАДАЮТСЯ И ВОЗВРАЩАЮТСЯ В ФОРМАТЕ (ЗНАЧЕНИЕ * 1000)!


typedef struct _adt7461_t
{
    thermoif_t      local_temp_sensor;
    thermoif_t      external_temp_sensor;
    
    i2cif_t         *i2c;
    i2c_message_t   msg;
    int             standby;
} adt7461_t;

//
// Функция-обёртка для получения указателя на интерфейс внутреннего термодатчика ADT7461.
//
static inline __attribute__((always_inline)) 
thermoif_t *adt7461_loc_sensor(adt7461_t *adt)
{
    return &adt->local_temp_sensor;
}

//
// Функция-обёртка для получения указателя на интерфейс внешнего термодатчика ADT7461.
//
static inline __attribute__((always_inline)) 
thermoif_t *adt7461_ext_sensor(adt7461_t *adt)
{
    return &adt->external_temp_sensor;
}

//
// Функция инициализации драйвера ADT7461. Нельзя вызывать из uos_init(),
// поскольку в ней осуществляется передача по I2C, с большой вероятностью
// закрытая мьютексом.
// i2c          - указатель на драйвер шины i2c.
// i2c_addr     - адрес микросхемы на шине i2c (старшие 7 битов).
// i2c_freq_khz - бодовая частота передачи на шине i2c в кГц.
// standby      - логический признак, если true, то микросхема инициализируется в режиме StandBy.
// Возвращает код завершения в соответствии с thermo-interface.h.
//
int adt7461_init(adt7461_t *adt7461, i2cif_t *i2c, uint8_t i2c_addr, unsigned i2c_freq_khz, int standby);

//
// Функция установки периода опроса АЦП.
// enum_conv_period - значение из перечисления ниже.
// Возвращает код завершения в соответствии с thermo-interface.h.
//
enum
{
    ADT7461_CONV_PERIOD_16,         // 16 с
    ADT7461_CONV_PERIOD_8,          // 8 с
    ADT7461_CONV_PERIOD_4,          // 4 с
    ADT7461_CONV_PERIOD_2,          // 2 с
    ADT7461_CONV_PERIOD_1,          // 1 с
    ADT7461_CONV_PERIOD_0_5,        // 0.5 с
    ADT7461_CONV_PERIOD_0_25,       // 0.25 с
    ADT7461_CONV_PERIOD_0_125,      // 0.125 с
    ADT7461_CONV_PERIOD_0_0625,     // 0.0625 с
    ADT7461_CONV_PERIOD_0_03125,    // 0.03125 с
    ADT7461_CONV_PERIOD_0_015625,   // 0.015625 с
    ADT7461_NB_OF_CONV_PERIODS
};
int adt7461_set_conversion_period(adt7461_t *adt7461, uint8_t enum_conv_period);

//
// Функция установки режима вывода ALERT/THERM2 микросхемы ADT7461.
// pin_mode     - режим ножки (ALERT или THERM2), используйте макроопределения ниже.
// nb_debounce  - число чтений температуры вне диапазона до формирования тревожного сигнала
//                (ALERT или THERM2). Число в диапазоне 1..4.
// Возвращает код завершения в соответствии с thermo-interface.h.
//
#define ADT7461_PIN_MODE_ALERT      (0 << 5)
#define ADT7461_PIN_MODE_THERM2     (1 << 5)
int adt7461_set_alert_mode(adt7461_t *adt7461, int pin_mode, int nb_debounce);

//
// Функция установки гистерезиса по включению/выключению ножек THERM и THERM2.
// therm_hyst   - значение гистерезиса в градусах, умноженных на 1000.
// Возвращает код завершения в соответствии с thermo-interface.h.
//
int adt7461_set_therm_hysteresis(adt7461_t *adt7461, int therm_hyst);

//
// Функция установки режима термостата на внутреннем датчике.
// low_limit   - нижний порог срабатывания (в градусах, умноженных на 1000).
// high_limit  - верхний порог срабатывания (в градусах, умноженных на 1000).
// therm_limit - порог срабатывания ножки THERM (в градусах, умноженных на 1000).
// Возвращает код завершения в соответствии с thermo-interface.h.
//
int adt7461_set_local_thermostat(adt7461_t *adt7461, int low_limit, int high_limit, int therm_limit);

//
// Функция установки режима термостата на внешнем датчике.
// low_limit   - нижний порог срабатывания (в градусах, умноженных на 1000).
// high_limit  - верхний порог срабатывания (в градусах, умноженных на 1000).
// therm_limit - порог срабатывания ножки THERM (в градусах, умноженных на 1000).
// Возвращает код завершения в соответствии с thermo-interface.h.
//
int adt7461_set_external_thermostat(adt7461_t *adt7461, int low_limit, int high_limit, int therm_limit);

//
// Функция установки поправки чтения внешнего датчика.
// ext_offset   - смещение температуры (число, прибавляемое к считанному значению 
//                с датчика); в градусах, умноженных на 1000.
// Возвращает код завершения в соответствии с thermo-interface.h.
//
int adt7461_set_external_offset(adt7461_t *adt7461, int ext_offset);

//
// Переключение микросхемы в режим StandBy. В этом режиме выключен периодический
// опрос датчиков, соответственно, не работают термостаты,
// но возможно разовое чтение температур по запросу (данный драйвер
// учитывает эту особенность микросхемы ADT7461, поэтому чтение температур
// работает правильно в режиме StandBy).
// Возвращает код завершения в соответствии с thermo-interface.h.
//
int adt7461_standby(adt7461_t *adt7461);

//
// Переключение микросхемы в рабочий режим.
// Возвращает код завершения в соответствии с thermo-interface.h.
//
int adt7461_wakeup(adt7461_t *adt7461);


#endif // __ADT7461_H__
