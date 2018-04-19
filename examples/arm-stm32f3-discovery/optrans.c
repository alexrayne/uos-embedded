#include <runtime/lib.h>
#include <kernel/uos.h>
#include <timer/timer.h>
#include <timer/timeout.h>
#include <stm32f3/gpio.h>
#include <stm32f3/i2c.h>
#include <stm32f3/can.h>
#include <stm32f3/uart.h>
#include <1-wire/1-wire-over-uart.h>
#include <thermo/adt7461.h>
#include <thermo/ds7505.h>
#include <thermo/ds18s20.h>

#define TRANS_TYPE_COMMANDER

#define CAN_SPEED                   500000
#ifdef TRANS_TYPE_COMMANDER
#   define CAN_ID_COMMAND           0x2E0
#   define CAN_ID_RESPONSE          0x490
#else
#   define CAN_ID_COMMAND           0x2F0
#   define CAN_ID_RESPONSE          0x4D0
#endif

#define I2C_FREQ                    100000
#define I2C_PORT_NUMBER             1

#define UART_PORT                   3

#define BK2_I2C_ADDRESS             0x90
#define BK3_I2C_ADDRESS             0x92
#define BK4_I2C_ADDRESS             0x94
#define BK5_I2C_ADDRESS             0x98

#define BK2_T_OS                    -40000
#define BK2_T_HYST                  -55000

#define TEMPORAL_FAIL_TS(sens_n)    (1 << ((sens_n) * 2))
#define PERMANENT_FAIL_TS(sens_n)   (2 << ((sens_n) * 2))
#define PERMANENT_FAIL_POWER        (1 << 16)

#define LOWER_TEMPERATURE_LIMIT     -40000
#define HIGHER_TEMPERATURE_LIMIT     35000

#define FPGA_RECONF_TIME            5000

#define CONTROLLING_PERIOD          1000

#define CAN_COMMAND_PRIO            20
#define CONTROLLING_PRIO            10

ARRAY (controlling_space, 800);
ARRAY (can_command_space, 800);

timer_t timer;
timeout_t controlling_timer;
mutex_t   controlling_mutex;

stm32f3_uart_t          uart;
stm32f3_i2c_t           i2c;
one_wire_over_uart_t    one_wire;
stm32f3_can_t           can;

ds18s20_t   bk1;
ds7505_t    bk2;
ds7505_t    bk3;
ds7505_t    bk4;
adt7461_t   bk5;

enum {
    BK1_IDX,
    BK2_IDX,
    BK3_IDX,
    BK4_IDX,
    BK5_LOC_IDX,
    BK6_EXT_IDX,
    MAX_BOARD_SENSORS
};
thermoif_t *board_sensor[MAX_BOARD_SENSORS];

unsigned dev_status;
int low_temp;
int dcdc_started;
int fpga_configured;
unsigned fpga_reconf_start_time;

stm32f3_gpio_t pin_permanent_on;
stm32f3_gpio_t pin_led_vd5;
stm32f3_gpio_t pin_led_vd6;
stm32f3_gpio_t pin_en_dcdc;
stm32f3_gpio_t pin_board_heat;
stm32f3_gpio_t pin_heat_sfp;
stm32f3_gpio_t pin_one_wire_rx;
stm32f3_gpio_t pin_one_wire_tx;
stm32f3_gpio_t pin_fpga_crc_error;
stm32f3_gpio_t pin_fpga_ld_done;
stm32f3_gpio_t pin_fpga_reconf;
stm32f3_gpio_t pin_pg_1v0;
stm32f3_gpio_t pin_pg_1v8;
stm32f3_gpio_t pin_pg_2v5;
stm32f3_gpio_t pin_pg_3v3;
stm32f3_gpio_t pin_can_init;
stm32f3_gpio_t pin_can_en;
stm32f3_gpio_t pin_vidmux;
stm32f3_gpio_t pin_i2c_sda;
stm32f3_gpio_t pin_i2c_scl;
stm32f3_gpio_t pin_can_rx;
stm32f3_gpio_t pin_can_tx;


void init_gpio()
{
    stm32f3_gpio_init(&pin_permanent_on,    GPIO_PORT_F,  1, GPIO_FLAGS_OUTPUT);
    stm32f3_gpio_init(&pin_led_vd5,         GPIO_PORT_B,  5, GPIO_FLAGS_OUTPUT);
    stm32f3_gpio_init(&pin_led_vd6,         GPIO_PORT_C, 13, GPIO_FLAGS_OUTPUT);
    stm32f3_gpio_init(&pin_en_dcdc,         GPIO_PORT_B,  6, GPIO_FLAGS_OUTPUT);
    stm32f3_gpio_init(&pin_board_heat,      GPIO_PORT_C, 14, GPIO_FLAGS_OUTPUT);
    stm32f3_gpio_init(&pin_heat_sfp,        GPIO_PORT_C, 15, GPIO_FLAGS_OUTPUT);
    stm32f3_gpio_init(&pin_one_wire_rx,     GPIO_PORT_B, 11, 
        GPIO_FLAGS_ALT_FUNC(7));                                                // Требуется доработка платы!
    stm32f3_gpio_init(&pin_one_wire_tx,     GPIO_PORT_B, 10, 
        GPIO_FLAGS_ALT_FUNC(7) | GPIO_FLAGS_OPEN_DRAIN | GPIO_FLAGS_PULL_UP);   // Требуется доработка платы!
    stm32f3_gpio_init(&pin_fpga_crc_error,  GPIO_PORT_B, 12, GPIO_FLAGS_INPUT);
    stm32f3_gpio_init(&pin_fpga_ld_done,    GPIO_PORT_B,  7, GPIO_FLAGS_INPUT);
    stm32f3_gpio_init(&pin_fpga_reconf,     GPIO_PORT_B,  9, GPIO_FLAGS_OUTPUT);
    stm32f3_gpio_init(&pin_pg_1v0,          GPIO_PORT_B, 15, GPIO_FLAGS_INPUT);
    stm32f3_gpio_init(&pin_pg_1v8,          GPIO_PORT_B, 13, GPIO_FLAGS_INPUT);
    stm32f3_gpio_init(&pin_pg_2v5,          GPIO_PORT_B, 14, GPIO_FLAGS_INPUT);
    stm32f3_gpio_init(&pin_pg_3v3,          GPIO_PORT_A,  8, GPIO_FLAGS_INPUT);
    stm32f3_gpio_init(&pin_can_init,        GPIO_PORT_B,  3, GPIO_FLAGS_OUTPUT);
    stm32f3_gpio_init(&pin_can_en,          GPIO_PORT_B,  4, GPIO_FLAGS_OUTPUT);
    stm32f3_gpio_init(&pin_vidmux,          GPIO_PORT_A, 13, GPIO_FLAGS_OUTPUT);
    stm32f3_gpio_init(&pin_i2c_sda,         GPIO_PORT_A, 14, GPIO_FLAGS_ALT_FUNC(4));
    stm32f3_gpio_init(&pin_i2c_scl,         GPIO_PORT_A, 15, GPIO_FLAGS_ALT_FUNC(4));
    stm32f3_gpio_init(&pin_can_rx,          GPIO_PORT_A, 11, GPIO_FLAGS_ALT_FUNC(9));
    stm32f3_gpio_init(&pin_can_tx,          GPIO_PORT_A, 12, GPIO_FLAGS_ALT_FUNC(9));
}

void init_controlling()
{
    int res;
    
    board_sensor[0] = to_thermoif(&bk1);
    board_sensor[1] = to_thermoif(&bk2);
    board_sensor[2] = to_thermoif(&bk3);
    board_sensor[3] = to_thermoif(&bk4);
    board_sensor[4] = adt7461_loc_sensor(&bk5);
    board_sensor[5] = adt7461_ext_sensor(&bk5);
    
    ds18s20_init(&bk1, to_owif(&one_wire), &timer, 800, 1);
    
    ds7505_init(&bk2, to_i2cif(&i2c), BK2_I2C_ADDRESS, I2C_MODE_FM, I2C_FREQ/1000);
    res = ds7505_set_mode(&bk2, DS7505_MODE_COMPARATOR | DS7505_OS_ACTIVE_LOW |
        DS7505_OS_DEBOUNCE_1 | DS7505_RESOLUTION_9BITS);
    if (res != THERMO_ERR_OK) {
        debug_printf("FAILED TO SET MODE FOR BK2 (DS7505)!\n");
        dev_status |= PERMANENT_FAIL_TS(1);
    }
    res = ds7505_set_thermostat(&bk2, BK2_T_OS, BK2_T_HYST);
    if (res != THERMO_ERR_OK) {
        debug_printf("FAILED TO SET THERMOSTAT BK2 (DS7505)!\n");
        dev_status |= PERMANENT_FAIL_TS(1);
    }
    
    ds7505_init(&bk3, to_i2cif(&i2c), BK3_I2C_ADDRESS, I2C_MODE_FM, I2C_FREQ/1000);
    res = ds7505_set_mode(&bk3, DS7505_MODE_COMPARATOR | DS7505_OS_ACTIVE_LOW |
        DS7505_OS_DEBOUNCE_1 | DS7505_RESOLUTION_9BITS);
    if (res != THERMO_ERR_OK) {
        debug_printf("FAILED TO SET MODE FOR BK3 (DS7505)!\n");
        dev_status |= PERMANENT_FAIL_TS(2);
    }
    
    ds7505_init(&bk4, to_i2cif(&i2c), BK4_I2C_ADDRESS, I2C_MODE_FM, I2C_FREQ/1000);
    res = ds7505_set_mode(&bk4, DS7505_MODE_COMPARATOR | DS7505_OS_ACTIVE_LOW |
        DS7505_OS_DEBOUNCE_1 | DS7505_RESOLUTION_9BITS);
    if (res != THERMO_ERR_OK) {
        debug_printf("FAILED TO SET MODE FOR BK4 (DS7505)!\n");
        dev_status |= PERMANENT_FAIL_TS(3);
    }
    
    res = adt7461_init(&bk5, to_i2cif(&i2c), BK5_I2C_ADDRESS, I2C_FREQ/1000, 1);
    if (res != THERMO_ERR_OK) {
        debug_printf("FAILED TO INIT BK5 (ADT7461)!\n");
        dev_status |= PERMANENT_FAIL_TS(4) | PERMANENT_FAIL_TS(5);
    }
}

void manage_thermo_sensor(int temperature, int thermo_idx)
{
    if (temperature <= LOWER_TEMPERATURE_LIMIT) {
        if (thermo_idx == BK1_IDX) {
            gpio_set_val(to_gpioif(&pin_heat_sfp), 1);
        } else {
            gpio_set_val(to_gpioif(&pin_board_heat), 1);
            low_temp = 1;
        }
    } else {
        low_temp = 0;
    }
    
    if (temperature >= HIGHER_TEMPERATURE_LIMIT) {
        if (thermo_idx == BK1_IDX)
            gpio_set_val(to_gpioif(&pin_heat_sfp), 0);
        else
            gpio_set_val(to_gpioif(&pin_board_heat), 0);
    }
}

void manage_heating()
{
    int res;
    
    int i;
    for (i = BK1_IDX; i < MAX_BOARD_SENSORS; ++i) {
        if (! (dev_status & PERMANENT_FAIL_TS(i))) {
            res = thermo_read(board_sensor[i]);
            if (res > 0x7FFFFFF0) {
                debug_printf("ERROR reading sensor #%d: %d!\n", i, res - 0x7FFFFFFF);
                dev_status |= TEMPORAL_FAIL_TS(i);
            } else {
                debug_printf("Sensor %d temperature:        %d\n", i, res);
                if ((res < -100) || (res > 100))
                    dev_status |= TEMPORAL_FAIL_TS(i);
                else
                    dev_status &= ~TEMPORAL_FAIL_TS(i);
            }
            
            if (! (dev_status & TEMPORAL_FAIL_TS(i)))
                manage_thermo_sensor(res, i);
        }
    }
}

void start_fgpa_reconf()
{
    gpio_set_val(to_gpioif(&pin_fpga_reconf), 1);
    gpio_set_val(to_gpioif(&pin_led_vd6), 0);
    fpga_reconf_start_time = timer_milliseconds(&timer);
    fpga_configured = 0;
}

void manage_power()
{
    if (!dcdc_started && (low_temp == 0)) {
        // Температура приемлемая и DC/DC ещё не стартовали -> стартуем DC/DC.
        gpio_set_val(to_gpioif(&pin_en_dcdc), 1);
        dcdc_started = 1;
        // Проверять состояние питания будем только на следующем цикле.
        return;
    }
    
    const int pg_1v0 = gpio_val(to_gpioif(&pin_pg_1v0));
    const int pg_1v8 = gpio_val(to_gpioif(&pin_pg_1v8));
    const int pg_2v5 = gpio_val(to_gpioif(&pin_pg_2v5));
    const int pg_3v3 = gpio_val(to_gpioif(&pin_pg_3v3));
    if (pg_1v0 && pg_1v8 && pg_2v5 && pg_3v3) {
        // С питанием всё хорошо. Если ПЛИС ещё не законфигурирована, конфигурируем.
        if (!fpga_configured)
            start_fgpa_reconf();
    } else {
        // Отказ питания
        if (dcdc_started) {
            // Отключаем DC/DC и переходим в перманентный отказ
            dev_status |= PERMANENT_FAIL_POWER;
            gpio_set_val(to_gpioif(&pin_en_dcdc), 0);
        }
    }
}

void manage_fpga()
{
    if (fpga_configured) {
        // ПЛИС уже сконфигурирована, следим за сигналами INT_B0 и DONE0.
        const int crc_ok  = gpio_val(to_gpioif(&pin_fpga_crc_error));
        const int ld_done = gpio_val(to_gpioif(&pin_fpga_ld_done));
        if (!crc_ok || !ld_done)
            start_fgpa_reconf(); // ПЛИС сигнализирует об отказе прошивки -> переконфигурируем.
        else
            gpio_set_val(to_gpioif(&pin_led_vd6), 1);   // Всё хорошо, зажигаем VD6
    } else {
        // ПЛИС в процессе конфигурирования
        if (timer_passed(&timer, fpga_reconf_start_time, FPGA_RECONF_TIME)) {
            // Время конфигурирования вышло - снимаем сигнал FPGA_RECONF.
            // Анализировать сигналы состояния ПЛИС будем на следующем шаге.
            gpio_set_val(to_gpioif(&pin_fpga_reconf), 0);
            fpga_configured = 1;
        }
    }
}

//
// Задача, контролирующая состояния платы
//
void controlling(void *arg)
{
    init_controlling();

	for (;;) {
	    mutex_wait(&controlling_mutex);
	    
	    if (dev_status != 0) {
	        // Есть ошибки работы с датчиками - мигаем VD5
	        gpio_set_val(to_gpioif(&pin_led_vd5), !gpio_val(to_gpioif(&pin_led_vd5)));
	    } else {
	        // Периферия в порядке - VD5 горит
	        gpio_set_val(to_gpioif(&pin_led_vd5), 1);
	    }
	    
        manage_heating();
        manage_power();
        manage_fpga();
	}
}

//
// Задача, обрабатывающая команды по CAN
//
void can_command(void *arg)
{
    static canif_t *canif = to_canif(&can);
    static can_frame_t cmd_frame;
    static can_frame_t rsp_frame;
    
    rsp_frame.id = CAN_ID_RESPONSE;
    rsp_frame.dlc = 1;
    
    can_reset(canif);
    can_set_timing(canif, CAN_SPEED, 0);
    can_switch_mode(canif, IFACE_MODE_OPERATION);
    can_add_filter(canif, 0x7FF, CAN_ID_COMMAND);

    for (;;) {
        can_input(canif, &cmd_frame, 1);
        gpio_set_val(to_gpioif(&pin_vidmux), cmd_frame.data[0] & 1);
        rsp_frame.data[0] = cmd_frame.data[0] & 1;
        can_output(canif, &rsp_frame, 1);
    }
}

void uos_init (void)
{
    timer_init(&timer, KHZ, 100);
    timeout_init(&controlling_timer, &timer, &controlling_mutex);
    timeout_set_value(&controlling_timer, CONTROLLING_PERIOD);
    timeout_set_autoreload(&controlling_timer, 1);

    init_gpio();
    
    stm32f3_i2c_init(&i2c, I2C_PORT_NUMBER);
    stm32f3_uart_init(&uart, UART_PORT);
    one_wire_over_uart_init(&one_wire, to_uartif(&uart));
    stm32f3_can_init(&can);
    
	task_create(controlling, 0, "cntr",   CONTROLLING_PRIO, controlling_space, sizeof (controlling_space));
	task_create(can_command, 0, "cancmd", CAN_COMMAND_PRIO, can_command_space, sizeof(can_command_space));
	
	// Сообщаем о завершении инициализации
	gpio_set_val(to_gpioif(&pin_permanent_on), 1);
}
