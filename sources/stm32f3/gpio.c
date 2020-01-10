#include <loadable/uos-interface.h>
#include "gpio.h"

extern uos_loadable_t *uos;

enum {
    HANDLER_EXTI0,
    HANDLER_EXTI1,
    HANDLER_EXTI2,
    HANDLER_EXTI3,
    HANDLER_EXTI4,
    HANDLER_EXTI9_5,
    HANDLER_EXTI15_10,
    HANDLER_MAX_NB
};

#define EXTI_HNDL_INIT  (1 << 30)

static int handler_state;
static list_t exti_hndl[HANDLER_MAX_NB];
static mutex_t exti_mutex[HANDLER_MAX_NB];
static const int irq_n[HANDLER_MAX_NB] = {
    IRQ_EXTI0, IRQ_EXTI1, IRQ_EXTI2_TS, IRQ_EXTI3, IRQ_EXTI4, 
    IRQ_EXTI9_5, IRQ_EXTI15_10
};


static void stm32f3_to_input(gpioif_t *pin)
{
    stm32f3_gpio_t *stm_pin = (stm32f3_gpio_t *)pin;
    stm_pin->reg->MODER &= ~GPIO_MODE_MASK(stm_pin->pin_n);
}
    
static void stm32f3_to_output(gpioif_t *pin)
{
    stm32f3_gpio_t *stm_pin = (stm32f3_gpio_t *)pin;
    stm_pin->reg->MODER = (stm_pin->reg->MODER & 
        ~GPIO_MODE_MASK(stm_pin->pin_n)) | GPIO_OUT(stm_pin->pin_n);
}
    
static void stm32f3_set_value(gpioif_t *pin, int v)
{
    stm32f3_gpio_t *stm_pin = (stm32f3_gpio_t *)pin;
    
    if (v)
        stm_pin->reg->BSRR = GPIO_SET(stm_pin->pin_n);
    else
        stm_pin->reg->BSRR = GPIO_RESET(stm_pin->pin_n);
}
    
static int stm32f3_value(gpioif_t *pin)
{
    stm32f3_gpio_t *stm_pin = (stm32f3_gpio_t *)pin;
    
    return (stm_pin->reg->IDR >> stm_pin->pin_n) & 1;
}

static bool_t stm32f3_gpio_irq_handler(void *arg)
{
    int handler_idx = (int)arg;
    stm32f3_gpio_hndl_list_item_t *item;
    
    list_iterate(item, &exti_hndl[handler_idx]) {
        if (EXTI->PR1 & EXTI_GPIO(item->pin->pin_n)) {
            item->pin->int_pending = 1;
            if (item->handler)
                item->handler(&item->pin->gpioif, item->handler_arg);
            EXTI->PR1 = EXTI_GPIO(item->pin->pin_n);
        }        
    }
    
    _UC(uos) arch_intr_allow(irq_n[handler_idx]);
    
    return 0;
}
    
static void stm32f3_attach_interrupt(gpioif_t *pin, unsigned event, 
        gpio_handler_t handler, void *arg)
{
    stm32f3_gpio_t *stm_pin = (stm32f3_gpio_t *)pin;
    arm_reg_t *exticr;
    int handler_idx = HANDLER_MAX_NB;
    
    switch (stm_pin->pin_n) {
    case 0:
        exticr = &SYSCFG->EXTICR1;
        handler_idx = HANDLER_EXTI0;
        break;
    case 1:
        exticr = &SYSCFG->EXTICR1;
        handler_idx = HANDLER_EXTI1;
        break;
    case 2:
        exticr = &SYSCFG->EXTICR1;
        handler_idx = HANDLER_EXTI2;
        break;
    case 3:
        exticr = &SYSCFG->EXTICR1;
        handler_idx = HANDLER_EXTI3;
        break;
    case 4:
        exticr = &SYSCFG->EXTICR2;
        handler_idx = HANDLER_EXTI4;
        break;
    case 5:
        exticr = &SYSCFG->EXTICR2;
        handler_idx = HANDLER_EXTI9_5;
        break;
    case 6:
        exticr = &SYSCFG->EXTICR2;
        handler_idx = HANDLER_EXTI9_5;
        break;
    case 7:
        exticr = &SYSCFG->EXTICR2;
        handler_idx = HANDLER_EXTI9_5;
        break;
    case 8:
        exticr = &SYSCFG->EXTICR3;
        handler_idx = HANDLER_EXTI9_5;
        break;
    case 9:
        exticr = &SYSCFG->EXTICR3;
        handler_idx = HANDLER_EXTI9_5;
        break;
    case 10:
        exticr = &SYSCFG->EXTICR3;
        handler_idx = HANDLER_EXTI15_10;
        break;
    case 11:
        exticr = &SYSCFG->EXTICR3;
        handler_idx = HANDLER_EXTI15_10;
        break;
    case 12:
        exticr = &SYSCFG->EXTICR4;
        handler_idx = HANDLER_EXTI15_10;
        break;
    case 13:
        exticr = &SYSCFG->EXTICR4;
        handler_idx = HANDLER_EXTI15_10;
        break;
    case 14:
        exticr = &SYSCFG->EXTICR4;
        handler_idx = HANDLER_EXTI15_10;
        break;
    case 15:
        exticr = &SYSCFG->EXTICR4;
        handler_idx = HANDLER_EXTI15_10;
        break;
    default:
        return;
    }
    
    assert(handler_idx < HANDLER_MAX_NB);
    
    RCC->APB2ENR |= RCC_SYSCFGEN;

    if (stm_pin->reg == GPIOA)
        *exticr = (*exticr & ~SYSCFG_PMASK(stm_pin->pin_n)) | SYSCFG_PA(stm_pin->pin_n);
    else if (stm_pin->reg == GPIOB)
        *exticr = (*exticr & ~SYSCFG_PMASK(stm_pin->pin_n)) | SYSCFG_PB(stm_pin->pin_n);
    else if (stm_pin->reg == GPIOC)
        *exticr = (*exticr & ~SYSCFG_PMASK(stm_pin->pin_n)) | SYSCFG_PC(stm_pin->pin_n);
    else if (stm_pin->reg == GPIOD)
        *exticr = (*exticr & ~SYSCFG_PMASK(stm_pin->pin_n)) | SYSCFG_PD(stm_pin->pin_n);
    else if (stm_pin->reg == GPIOE)
        *exticr = (*exticr & ~SYSCFG_PMASK(stm_pin->pin_n)) | SYSCFG_PE(stm_pin->pin_n);
    else if (stm_pin->reg == GPIOF)
        *exticr = (*exticr & ~SYSCFG_PMASK(stm_pin->pin_n)) | SYSCFG_PF(stm_pin->pin_n);
    else
        return;
        
    list_init(&stm_pin->hndl_item.item);
    stm_pin->hndl_item.pin = stm_pin;
    stm_pin->hndl_item.handler = handler;
    stm_pin->hndl_item.handler_arg = arg;
    stm_pin->irq_handler_idx = handler_idx;
        
    EXTI->IMR1 |= EXTI_GPIO(stm_pin->pin_n);
    if (event & GPIO_EVENT_RISING_EDGE)
        EXTI->RTSR1 |= EXTI_GPIO(stm_pin->pin_n);
    if (event & GPIO_EVENT_FALLING_EDGE)
        EXTI->FTSR1 |= EXTI_GPIO(stm_pin->pin_n);
        
    arch_state_t x;
    arch_intr_disable(&x);
    
    list_append(&exti_hndl[handler_idx], &stm_pin->hndl_item.item);
    
    if (! (handler_state & (1 << handler_idx))) {
        _UC(uos) mutex_attach_irq(&exti_mutex[handler_idx], irq_n[handler_idx], 
            stm32f3_gpio_irq_handler, (void *)handler_idx);
        handler_state |= (1 << handler_idx);
    }
    
    arch_intr_restore(x);
}

static void stm32f3_detach_interrupt(gpioif_t *pin)
{
    stm32f3_gpio_t *stm_pin = (stm32f3_gpio_t *)pin;
    
    arch_state_t x;
    arch_intr_disable(&x);
    
    list_unlink(&stm_pin->hndl_item.item);
    
    if (list_is_empty(&exti_hndl[stm_pin->irq_handler_idx])) {
        _UC(uos) mutex_lock(&exti_mutex[stm_pin->irq_handler_idx]);
        _UC(uos) mutex_unlock_irq(&exti_mutex[stm_pin->irq_handler_idx]);
    }
    
    arch_intr_restore(x);
}

static int stm32f3_interrupt_pending(gpioif_t *pin)
{
    stm32f3_gpio_t *stm_pin = (stm32f3_gpio_t *)pin;
    return stm_pin->int_pending;
}

static void stm32f3_clear_interrupt(gpioif_t *pin)
{
    stm32f3_gpio_t *stm_pin = (stm32f3_gpio_t *)pin;
    stm_pin->int_pending = 0;
}

static mutex_t *stm32f3_get_mutex(gpioif_t *pin)
{
    stm32f3_gpio_t *stm_pin = (stm32f3_gpio_t *)pin;
    return &exti_mutex[stm_pin->irq_handler_idx];
}

void stm32f3_gpio_init(stm32f3_gpio_t *gpio, unsigned port, unsigned pin_n, 
        unsigned flags)
{
    gpio->pin_n  = pin_n;
    
    switch (port) {
    case GPIO_PORT_A:
        RCC->AHBENR |= RCC_IOPAEN;
        gpio->reg = GPIOA;
        break;
    case GPIO_PORT_B:
        RCC->AHBENR |= RCC_IOPBEN;    
        gpio->reg = GPIOB;
        break;
    case GPIO_PORT_C:
        RCC->AHBENR |= RCC_IOPCEN;
        gpio->reg = GPIOC;
        break;
    case GPIO_PORT_D:
        RCC->AHBENR |= RCC_IOPDEN;
        gpio->reg = GPIOD;
        break;
    case GPIO_PORT_E:
        RCC->AHBENR |= RCC_IOPEEN;
        gpio->reg = GPIOE;
        break;
    case GPIO_PORT_F:
        RCC->AHBENR |= RCC_IOPFEN;
        gpio->reg = GPIOF;
        break;
    default:
        gpio->reg = 0;
    }
    
    switch (flags & _IO_MODE_MASK) {
    case 0: // альтернативная функция
        gpio->reg->MODER = (gpio->reg->MODER & ~GPIO_MODE_MASK(pin_n)) | GPIO_ALT(pin_n);
        if (pin_n < 8) {
            gpio->reg->AFRL = (gpio->reg->AFRL & ~GPIO_AF_MASK(pin_n)) | ((flags & _ALT_FUNC_MASK) << (4 * pin_n));
        } else {
            gpio->reg->AFRH = (gpio->reg->AFRH & ~GPIO_AF_MASK(pin_n)) | ((flags & _ALT_FUNC_MASK) << (4 * (pin_n - 8)));
        }
        break;
    case GPIO_FLAGS_INPUT:
        gpio->reg->MODER = (gpio->reg->MODER & ~GPIO_MODE_MASK(pin_n)) | GPIO_IN(pin_n);
        break;
    case GPIO_FLAGS_OUTPUT:
        gpio->reg->MODER = (gpio->reg->MODER & ~GPIO_MODE_MASK(pin_n)) | GPIO_OUT(pin_n);
        break;
    case GPIO_FLAGS_ANALOG:
        gpio->reg->MODER = (gpio->reg->MODER & ~GPIO_MODE_MASK(pin_n)) | GPIO_ANA(pin_n);
        break;
    }
        
    if (flags & GPIO_FLAGS_PULL_UP)
        gpio->reg->PUPDR |= GPIO_PULL_UP(pin_n);
    else
        gpio->reg->PUPDR &= ~GPIO_PULL_UP(pin_n);
        
    if (flags & GPIO_FLAGS_PULL_DOWN)
        gpio->reg->PUPDR |= GPIO_PULL_DOWN(pin_n);
    else
        gpio->reg->PUPDR &= ~GPIO_PULL_DOWN(pin_n);
        
    if (flags & GPIO_FLAGS_OPEN_DRAIN)
        gpio->reg->OTYPER |= GPIO_OD(pin_n);
    else
        gpio->reg->OTYPER &= ~GPIO_OD(pin_n);
        
    gpio->reg->OSPEEDR = (gpio->reg->OSPEEDR & ~GPIO_OSPEED_MASK(pin_n)) | (_GET_POWER(flags) << (2 * pin_n));
        
    gpioif_t *gpioif = &gpio->gpioif;
    
    gpioif->to_input = stm32f3_to_input;
    gpioif->to_output = stm32f3_to_output;
    gpioif->set_value = stm32f3_set_value;
    gpioif->value = stm32f3_value;
    gpioif->attach_interrupt = stm32f3_attach_interrupt;
    gpioif->detach_interrupt = stm32f3_detach_interrupt;
    gpioif->interrupt_pending = stm32f3_interrupt_pending;
    gpioif->clear_interrupt = stm32f3_clear_interrupt;
    gpioif->get_mutex = stm32f3_get_mutex;
    
    if (! (handler_state & EXTI_HNDL_INIT)) {
        int i;
        for (i = 0; i < HANDLER_MAX_NB; ++i)
            list_init(&exti_hndl[i]);
        handler_state |= EXTI_HNDL_INIT;
    }
}
