#ifndef __GPIO_STM32F3_H__
#define __GPIO_STM32F3_H__

#include <gpio/gpio-interface.h>

typedef struct _stm32f3_gpio_t stm32f3_gpio_t;
typedef struct _stm32f3_gpio_hndl_list_item_t stm32f3_gpio_hndl_list_item_t;

struct _stm32f3_gpio_hndl_list_item_t
{
    list_t              item;
    
    stm32f3_gpio_t       *pin;
    gpio_handler_t      handler;
    void                *handler_arg;
};

struct _stm32f3_gpio_t
{
    gpioif_t            gpioif;
    
    GPIO_t              *reg;
    unsigned            pin_n;
    
    int                 irq_handler_idx;
    int                 int_pending;
    
    stm32f3_gpio_hndl_list_item_t hndl_item;
};


typedef enum
{
    GPIO_PORT_A,
    GPIO_PORT_B,
    GPIO_PORT_C,
    GPIO_PORT_D,
    GPIO_PORT_E,
    GPIO_PORT_F,
    GPIO_PORT_G,
    GPIO_PORT_H
} gpio_port_t;

// Ножка настраивается в режим альтернативной функции
#define GPIO_FLAGS_ALT_FUNC(x)        (x)
// Ножка настраивается в режим цифрового входа
#define GPIO_FLAGS_INPUT              (1 << 4)
// Ножка настраивается в режим цифрового выхода
#define GPIO_FLAGS_OUTPUT             (2 << 4)
// Ножка настраивается в режим аналогового входа-выхода
#define GPIO_FLAGS_ANALOG             (3 << 4)

// Включение подтягивающего к питанию резистора
#define GPIO_FLAGS_PULL_UP            (1 << 6)
// Включение подтягивающего к земле резистора
#define GPIO_FLAGS_PULL_DOWN          (1 << 7)
// Режим работы цифрового выхода - открытый коллектор (если флаг не 
// указан, то push-pull; применимо только для цифрового выхода и 
// альтернативных функций)
#define GPIO_FLAGS_OPEN_DRAIN         (1 << 8)

// Крутизна фронта для цифровых выходов и альтернативных функций
#define GPIO_FLAGS_LOW_SPEED          (0 << 9)
#define GPIO_FLAGS_MEDIUM_SPEED       (1 << 9)
#define GPIO_FLAGS_HIGH_SPEED         (2 << 9)


// Служебные маски
#define _IO_MODE_MASK            0x030
#define _ALT_FUNC_MASK           0x00F
#define _GET_POWER(flags)        (((flags) >> 9) & 0x3)


void stm32f3_gpio_init(stm32f3_gpio_t *gpio, unsigned port, unsigned pin, unsigned flags);

#endif // __GPIO_STM32F3_H__
