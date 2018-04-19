/*
 * GPIO interface.
 *
 * Copyright (C) 2014-2015 Dmitry Podkhvatilin <vatilin@gmail.com>
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
#ifndef __GPIO_INTERFACE_H__
#define __GPIO_INTERFACE_H__


#define GPIO_EVENT_RISING_EDGE          (1 << 0)
#define GPIO_EVENT_FALLING_EDGE         (1 << 1)


typedef struct _gpioif_t gpioif_t;

typedef void (* gpio_handler_t) (gpioif_t *pin, void *arg);

//
// Базовый тип интерфейса GPIO
//
struct _gpioif_t
{
    void (* to_input)(gpioif_t *pin);
    
    void (* to_output)(gpioif_t *pin);
    
    void (* set_value)(gpioif_t *pin, int v);
    
    int (* value)(gpioif_t *pin);
    
    void (* attach_interrupt)(gpioif_t *pin, unsigned event, 
        gpio_handler_t handler, void *arg);
    
    void (* detach_interrupt)(gpioif_t *pin);
    
    int (* interrupt_pending)(gpioif_t *pin);
    
    void (* clear_interrupt)(gpioif_t *pin);
    
    mutex_t * (* get_mutex)(gpioif_t *pin);
};

#define to_gpioif(x)   ((gpioif_t*)&(x)->gpioif)

//
// Функции-обёрки для удобства вызова функций интерфейса.
//


static inline __attribute__((always_inline)) 
void gpio_to_input(gpioif_t *pin)
{
#ifdef GPIO_CHECK_FUNC_POINTERS
    if (pin->to_input)
#endif
    pin->to_input(pin);
}

static inline __attribute__((always_inline)) 
void gpio_to_output(gpioif_t *pin)
{
#ifdef GPIO_CHECK_FUNC_POINTERS
    if (pin->to_output)
#endif
    pin->to_output(pin);
}

static inline __attribute__((always_inline)) 
void gpio_set_val(gpioif_t *pin, int val)
{
#ifdef GPIO_CHECK_FUNC_POINTERS
    if (pin->set_value)
#endif
    pin->set_value(pin, val);
}

static inline __attribute__((always_inline)) 
int gpio_val(gpioif_t *pin)
{
#ifdef GPIO_CHECK_FUNC_POINTERS
    if (pin->value)
        return pin->value(pin);
    else
        return 0;
#else
    return pin->value(pin);
#endif
}

static inline __attribute__((always_inline)) 
void gpio_attach_interrupt(gpioif_t *pin, int event, 
        gpio_handler_t handler, void *arg)
{
#ifdef GPIO_CHECK_FUNC_POINTERS
    if (pin->attach_interrupt)
#endif
    pin->attach_interrupt(pin, event, handler, arg);
}

static inline __attribute__((always_inline)) 
void gpio_detach_interrupt(gpioif_t *pin)
{
#ifdef GPIO_CHECK_FUNC_POINTERS
    if (pin->detach_interrupt)
#endif
    pin->detach_interrupt(pin);
}

static inline __attribute__((always_inline)) 
mutex_t *gpio_get_mutex(gpioif_t *pin)
{
#ifdef GPIO_CHECK_FUNC_POINTERS
    if (pin->get_mutex)
        return pin->get_mutex(pin);
    else
        return 0;
#else
    return pin->get_mutex(pin);
#endif
}

static inline __attribute__((always_inline)) 
int gpio_interrupt_pending(gpioif_t *pin)
{
#ifdef GPIO_CHECK_FUNC_POINTERS
    if (pin->interrupt_pending)
        return pin->interrupt_pending(pin);
    else
        return 0;
#else
    return pin->interrupt_pending(pin);
#endif
}

static inline __attribute__((always_inline)) 
void gpio_clear_interrupt(gpioif_t *pin)
{
#ifdef GPIO_CHECK_FUNC_POINTERS
    if (pin->clear_interrupt)
#endif
    pin->clear_interrupt(pin);
}

static inline __attribute__((always_inline)) 
void gpio_wait_irq(gpioif_t *pin)
{
    mutex_wait(gpio_get_mutex(pin));
}


#endif
