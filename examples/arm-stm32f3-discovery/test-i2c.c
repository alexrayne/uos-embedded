//
// Тестирование I2C
//
#include <runtime/lib.h>
#include <kernel/uos.h>
#include <stm32f3/i2c.h>
#include <stm32f3/gpio.h>

#define SLA     0x90
#define FREQ    400000

ARRAY (task_space, 0x400);

stm32f3_i2c_t i2c;
stm32f3_gpio_t sda;
stm32f3_gpio_t scl;

i2c_message_t msg;
i2c_transaction_t trans1;
i2c_transaction_t trans2;
uint8_t data1[2];
uint8_t data2[2];

void task (void *arg)
{
    int res;

    data1[0] = 3;
    trans1.data = data1;
    trans1.size = 1;
    trans1.next = &trans2;
    
    trans2.data = data2;
    trans2.size = -1;
    trans2.next = 0;
            
    msg.mode = I2C_MODE_SLAVE_ADDR(SLA) | I2C_MODE_7BIT_ADDR | 
        I2C_MODE_SM | I2C_MODE_FREQ_KHZ(FREQ/1000);
    msg.first = &trans1;
    
    for (;;) {
        res = i2c_trx( to_i2cif(&i2c), &msg);
        debug_printf("i2c_trx result: %d, mant1: %d, frac1: %X, mant2: %d\n", res, data1[0], data1[1], data2[0]);
        mdelay(500);
    }
}

void uos_init (void)
{
    debug_printf("\n\nSTM32F3 I2C\n");

    stm32f3_gpio_init(&scl, GPIO_PORT_B, 6, GPIO_FLAGS_ALT_FUNC(4));
    stm32f3_gpio_init(&sda, GPIO_PORT_B, 7, GPIO_FLAGS_ALT_FUNC(4));
    stm32f3_i2c_init(&i2c, 1);
    
	task_create( task, "task", "task", 1, task_space, sizeof(task_space) );
}
