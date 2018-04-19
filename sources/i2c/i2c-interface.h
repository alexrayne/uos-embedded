/*
 * I2C interface.
 *
 * Copyright (C) 2018 Dmitry Podkhvatilin <vatilin@gmail.com>
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You can redistribute this file and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software Foundation;
 * either version 2 of the License, or (at your discretion) any later version.
 * See the accompanying file "COPYING.txt" for more details.
 *
 * As a special exception to the GPL, permission is granted for additional
 * uses of the text contained in this file.  See the accompanying file
 * "COPY-UOS.txt" for details.
 */
#ifndef __I2C_INTERFACE_H__
#define __I2C_INTERFACE_H__

// !!! ПЕРЕПИСАТЬ ДЛЯ ДРАЙВЕРА I2C

//
//        Интерфейс драйвера контроллера I2C
//
//    Данный заголовочный файл предназначен для унификации интерфейса драйверов
// контроллеров SPI различных производителей. Унификация интерфейса нужна для
// возможности написания ПЕРЕНОСИМЫХ драйверов контроллеров более высокого 
// уровня, использующих SPI в качестве технологической линии связи (например, 
// драйвер флеш-памяти SD в режиме SPI, драйверы различных EEPROM, подключаемых 
// по SPI, драйвер USB-контроллера MAX3421 и т.д.).
//
//    Для разработки драйвера SPI, совместимого с данным интерфейсом, необходимо
// выполнить несколько шагов:
//
// 1) Объявить структуру драйвера SPI, "наследующую" данную структуру, т.е.
// содержащую в качестве первого поля переменную структуры spimif_t, например:
//
//    struct _manufacturer_spim_t {
//
//       spimif_t        spimif;
//
//       ... // другие поля, необходимые для драйвера SPI.
//    };
//    typedef struct _manufacturer_spim_t manufacturer_spim_t;
//
// Имя структуры, разумеется, может быть произвольным.
//
// 2) Написать функцию приёма-передачи для данного контроллера SPI вида:
//
//    int manufacturer_trx (spimif_t *spi, spi_message_t *msg)
//    {
//          // Преобразуем указатель к структуре нашего аппаратного драйвера
//          // (мы уверены, что нам передают указатель на правильную структуру).
//          manufacturer_spim_t *mspi = (manufacturer_spim_t *) spi;
//
//          ... // Код, реализующий обмен. Используется структура аппаратного
//              // драйвера для хранения нужной для работы информации.
//    }
//
// Необходимо учитывать, что информация о скорости передачи и режиме работы
// контроллера содержится в структуре сообщения spi_message_t, а не в структуре
// драйвера.
// Функция должна возвращать коды результата, объявленные в данном файле.
// Если поле msg->tx_data равно 0, то в линию должно быть выдано msg->word_count
// нулей.
// Если поле msg->rx_data равно 0, то принятые данные должны быть отброшены.
//
// 3) Написать функцию инициализации драйвера SPI. Параметры могут быть любыми,
// данным интерфейсом они никак не регламентируются. Но логично, что одним из
// параметров должен быть указатель на структуру драйвера, содержащую в себе 
// структуру spimif_t. Таким образом, функция инициализации должна выглядеть
// примерно так:
//
//    int manuf_spi_init(manufacturer_spim_t *spi, ...)
//    {
//          spi->spimif.trx = manufacturer_trx;
//          ...
//    }
//
// В функции инициализации обязательно должен быть установлен указатель на 
// функцию приёма-передачи.
//
// 4) Для драйверов SPI предусмотрено макроопределение SPI_NO_DMA, которое,
// если задано, предполагает отключение использования каналов прямого доступа 
// в драйвере SPI. Реализовывать ли такую реакцию на данное макроопределение
// или нет - решение лежит на разработчике конкретного драйвера SPI.
//
// В качестве примеров можно использовать драйверы SPI для процессоров фирмы
// Элвис и Миландр: sources/elvees/spi.*, sources/milandr/spi.* .
//
//
//    Использование драйвера:
//
// 1) Объявить переменную структуры драйвера SPI:
//      manufacturer_spim_t spim;
// 2) Вызвать для неё функцию инициализации. Логично, если это произойдёт в
// функции uos_init(), хотя это и не обязательно:
//      void uos_init()
//      {
//          manuf_spi_init(&spim, ...);
//          ...
//      }
// 3) Объявить переменную сообщения SPI. Проинициализировать её поля и вызвать
// функцию spim_trx:
//
//      spi_message_t msg;
//      uint8_t rxbuf[16];  // буфер для принятых данных
//      uint8_t txbuf[16];  // буфер для выдаваемых данных
//
//      ...
//
//      msg.rx_data = rxbuf;
//      msg.tx_data = txbuf;
//      msg.word_count = 16;
//      msg.freq = 4000000; // битовая скорость в герцах
//      msg.mode = SPI_MODE_CPHA | SPI_MODE_NB_BITS(8); // это для примера
//
//      spim_trx((spimif_t *) &spim, &msg);
//
// Возможные установки режима приёма-передачи приведены ниже в данном файле.
// Поля rx_data и tx_data сообщения могут быть нулевыми, если, соответственно,
// принятые данные не представляют интереса или передаваемые данные могут быть
// просто нулевыми.
//


//
// Коды результата
//

// Успешно
#define I2C_ERR_OK              0
// Возвращается в случае передачи функции неверного параметра.
#define I2C_ERR_BAD_PARAM       -1
// Возвращается в случае, если запрошена неподдерживаемая драйвером
// скорость передачи (битовая частота) в данном режиме (SM, SMBUS, FM и т.д.).
#define I2C_ERR_BAD_FREQ        -2
// Возвращается, если сообщение слишком длинное для драйвера.
#define I2C_ERR_TOO_LONG        -4
// Возвращается, если указан недопустимый номер порта (контроллера) SPI.
#define I2C_ERR_BAD_PORT        -5
// Возвращается, если запрошен неподдерживаемый режим передачи.
#define I2C_ERR_MODE_NOT_SUPP   -6
// Возвращается, если ведомое устройство не подтвердило выполнение транзакции
#define I2C_ERR_NOT_ACKED       -7
// Возвращается, если шина занята
#define I2C_ERR_BUSY            -8
// Возвращается, если I2C-мастер проиграл арбитраж
#define I2C_ERR_ARBITR_LOST     -9
// Возвращается при на наличии недопустимого состояния на шине
#define I2C_ERR_BUS             -10


typedef struct _i2cif_t i2cif_t;
typedef struct _i2c_message_t i2c_message_t;
typedef struct _i2c_transaction_t i2c_transaction_t;


//
// Базовый тип интерфейса I2C
//
struct _i2cif_t
{
    // Мьютекс для синхронизации.
    mutex_t     lock;
    
    // В данное поле должен быть записан указатель на аппаратно-зависимую
    // функцию, выполняющую приём-передачу по интерфейсу i2c сообщения msg.
    int (* trx)(i2cif_t *i2c, i2c_message_t *msg);
};

#define to_i2cif(x)   ((i2cif_t*)&(x)->i2cif)

//
// Структура сообщения I2C
//
struct _i2c_message_t
{
    // Адрес ведомого устройства и режим.
    unsigned    mode;
    // Указатель на первую транзакцию.
    i2c_transaction_t   *first;
};

struct _i2c_transaction_t
{
    // Следующая транзакция.
    i2c_transaction_t   *next;
    // Указатель на буфер с данными.
    void                *data;
    // Размер передачи.
    // Положительное число - количество байт для передачи.
    // Отрицательное число - количество байт для приёма.
    // Ноль - драйвер должен пропустить данную транзакцию и перейти к следующей.
    int                 size;
#define I2C_TX_SIZE(x)  (x)
#define I2C_RX_SIZE(x)  (-(x))
};

// Макросы для задания адреса ведомого устройства и режима передачи
#define I2C_MODE_SLAVE_ADDR(x)      (x)             // Адрес ведомого
#define I2C_MODE_GET_SLAVE_ADDR(r)  ((r) & 0x3FF)   // Получение адреса из поля mode
#define I2C_MODE_7BIT_ADDR          (0 << 10)       // 7-битный адрес (по умолчанию)
#define I2C_MODE_10BIT_ADDR         (1 << 10)       // 10-битный адрес

#define I2C_MODE_SM                 (0 << 12)       // Времянка Standard-mode
#define I2C_MODE_SMBUS              (1 << 12)       // Времянка SMBUS
#define I2C_MODE_FM                 (2 << 12)       // Времянка Fast-mode
#define I2C_MODE_FM_PLUS            (3 << 12)       // Времянка Fast-mode+
#define I2C_MODE_UFM                (4 << 12)       // Времянка Ultra Fast-mode
#define I2C_MODE_MASK               (0x7 << 12)     // Маска режима (времянки)

#define I2C_MODE_FREQ_KHZ(x)        ((x) << 16)             // Частота передачи в кГц
#define I2C_MODE_GET_FREQ_KHZ(r)    (((r) >> 16) & 0xFFFF)  // Получение частоты из поля mode

//
// Функция-обёрка для удобства вызова функции приёма-передачи.
//
static inline __attribute__((always_inline)) 
int i2c_trx(i2cif_t *i2c, i2c_message_t *msg)
{
    return i2c->trx(i2c, msg);
}

#endif // __I2C_INTERFACE_H__
