
//
// Инициализация драйвера Ethernet
//
//  u       указатель на структуру, описывающую интерфейс
//  name    символическое имя интерфейса
//  prio    приоритет задачи-обработчика прерываний по выдаче
//          Приоритет задачи-обработчика прерываний по приёму
//          принимается равным (prio + 1)
//  pool    указатель на пул динамической памяти, используемый
//          драйвером Ethernet
//  arp     указатель на структуру ARP
//  macaddr указатель на массив 6 байтов, содержащий MAC-адрес
//          устройства
//
// Порядок вызова функций инициализации
// mem_init(&pool, (unsigned) __bss_end, (unsigned) _estack - 256);
// timer_init (&timer, KHZ, 50);
// mutex_group_t *g = mutex_group_init (group, sizeof(group));
// mutex_group_add(g, &eth->netif.lock);
// mutex_group_add(g, &timer.decisec);
// arp = arp_init(...);
// ip_init(...);
// route_add_netif(...);
// create_eth_interrupt_task(...);
// eth_init(...);

#ifndef __VE1ETH_H
#define __VE1ETH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <net/ip.h>
#include <net/tcp.h>
#include <net/netif.h>
#include <buf/buf-queue.h>

// Для использования DMA доступны только программные запросы
// Выдержка из форума АО "ПКК Миландр":
// heiwind:
// Тогда, раз аппаратных запросов на цикл DMA блок Ethernet не устанавливает, не очень понятно как пользоваться EVNT[1] и EVNT[0], 
// описанных в параграфе 30.8 События приемника и передатчика, вы могли бы вкратце пояснить?
// Petr (АО "ПКК Миландр"):
// К сожалению, никак.
// В блоке Ethernet_MAC возможность подключения к DMA есть, поэтому описаны настройки событий для генерирования запросов на
// выполнение цикл DMA, но к самому блоку DMA данные выводы не подключены (сигналы не доходят)...
// http://forum.milandr.ru/viewtopic.php?p=14732#p14732

#ifndef ETH_DMA_CHN_TX
#define ETH_DMA_CHN_TX           7//31//30//0
#endif

#ifndef ETH_DMA_CHN_RX
#define ETH_DMA_CHN_RX           1//29//1
#endif

//Для работы без прерываний с poll-функциями
//#define ETH_POLL_MODE 

#define ARM_ETH_PHY_ADDRESS      0x1C
#define ARM_ANALOG_MASK(n)      (1 << (n))
#define ARM_OE_MASK(n)          (1 << (n))
#define ARM_GFEN_MASK           (1 << (n))
#define ARM_PD_MASK             ((1 << (n)) | (1 << (n)*2))

// наличие данных в буфере
#define R_Buff_Has_Eth_Frame()  (ARM_ETH->R_HEAD != ARM_ETH->R_TAIL)
#define X_Buff_Is_Empty()       (ARM_ETH->X_TAIL == ARM_ETH->X_HEAD)

// полный размер ethernet-буффера
#define ARM_ETH_BUF_FULL_SIZE   8192

// половина буффера
#define ARM_ETH_BUF_HALF        (ARM_ETH_BUF_FULL_SIZE >> 1)

// размер буффера приемника
#if defined (ETH_BUFTYPE_FIFO) || (! ETH_BUF_SIZE_R)
#   undef  ETH_BUF_SIZE_R
#   define ETH_BUF_SIZE_R       ARM_ETH_BUF_HALF 
#endif
#define ARM_ETH_BUF_SIZE_R      ETH_BUF_SIZE_R

// размер буффера передатчика
#define ARM_ETH_BUF_SIZE_X      (ARM_ETH_BUF_FULL_SIZE-ARM_ETH_BUF_SIZE_R)

// адреса начала буферов приемника и передатчика
#define ARM_ETH_BUF_BASE_R      ARM_ETH_BUF_BASE
#define ARM_ETH_BUF_BASE_X      (ARM_ETH_BUF_BASE + ARM_ETH_BUF_SIZE_R)

// значение регистра задания предделителя шага изменения BAG и JitterWnd в мкс
#define PSC_VAL(v)              (KHZ * (v) - 1)
// значение для регистра периода следования пакетов в мкс
#define BAG_PERIOD(t)           (KHZ * (t))
// значение для регистра джиттера при передачи пакетов в мкс
#define JITTER_WND(t)           (KHZ * (t) - 1)

// количество попыток обмена
#define EXCH_ATTEMPT            10
// размер окна коллизий
#define COLL_WND                ((uint32_t)(1 << 7))

//#define ETH_INQ_SIZE          TCP_SOCKET_QUEUE_SIZE
//#define ETH_INQ_SBYTES        2048 //ARM_ETH_BUF_SIZE_R  // допустимое кол-во байт в очереди
//#define ETH_OUTQ_SIZE         TCP_SND_QUEUELEN
//#define ETH_OUTQ_SBYTES       2048 //ARM_ETH_BUF_SIZE_X  // допустимое кол-во байт в очереди     

#ifndef ETH_STACKSZ
#   define ETH_STACKSZ          1000  // Размер стека задачи-обработчика
#endif

#ifndef ETH_INQ_SIZE
#   define ETH_INQ_SIZE         16
#endif

#ifndef ETH_OUTQ_SIZE
#   define ETH_OUTQ_SIZE        8
#endif

#ifndef ETH_MTU
#   define ETH_MTU              1518  // максимальная длина ethernet-включая заголовки и CRC (без преамбулы и сепаратора)
#endif

// Если размер буфера передатчика не вмещает IP-пакет(TCP_MSS+60) и два служебных слова(+8)
#if (ARM_ETH_BUF_SIZE_X < (TCP_MSS + 60 + 8))
#error "ARM_ETH_BUF_SIZE_X must be greater than or equal (TCP_MSS + 60 + 8), to reduce ETH_BUF_SIZE_R"
#endif

#if (ARM_ETH_BUF_SIZE_R < (TCP_MSS * 2))
#error "ARM_ETH_BUF_SIZE_R must be greater than or equal (TCP_MSS * 2), to increase ETH_BUF_SIZE_R"
#endif

extern task_t *eth_task;

typedef enum {RESET = 0, SET = !RESET} FlagStatus, ITStatus;
void eth_controller_init(const uint8_t* MACAddr, uint8_t PHYMode);
void ethernet_led(void);

typedef void (* interrupt_handler_t) (void *);

typedef struct _intr_handler_t {
    list_t                  item;
    uint32_t                mask0;
    uint32_t                mask1;
    interrupt_handler_t     handler;
    void *                  handler_arg;
    mutex_t *               handler_lock;
} intr_handler_t;

struct _mem_pool_t;
struct _stream_t *stream;

typedef struct _eth_t {
    netif_t netif;                  /* common network interface part */
    mutex_t tx_lock;                /* get tx interrupts here */
    struct _mem_pool_t *pool;       /* memory pool for allocating packets */

    buf_queue_t inq;                /* queue of received packets */
    struct _buf_t *inqdata[ETH_INQ_SIZE];

    buf_queue_t outq;               /* queue of packets to transmit */
    struct _buf_t *outqdata[ETH_OUTQ_SIZE];

    uint8_t phy;                   /* address of external PHY */
    uint32_t intr;                 /* interrupt counter */
    uint8_t *rxbuf;                /* aligned rxbuf[] */
    uint8_t *txbuf;                /* aligned txbuf[] */
    uint32_t rxbuf_physaddr;       /* phys address of rxbuf[] */
    uint32_t txbuf_physaddr;       /* phys address of txbuf[] */
    arm_reg_t ifr_reg;
    char  *mut_signal;
    
    intr_handler_t irq_rx;
    intr_handler_t irq_tx;
    
    uint32_t rx_ovf;
    
} eth_t;

void eth_led(void);

// Инициализация задачи-обработчика прерываний Ethernet
// 
// вызывается до eth_init(), иначе будет  hard fault
//  
//  
void create_eth_interrupt_task (eth_t *u, int prio, void *stack, int stacksz); 

// Инициализация драйвера Ethernet
// 
// вызывается после create_eth_interrupt_task() 
// возможные значения phy_mode:
// ARM_ETH_PHY_10BASET_HD_NOAUTO
// ARM_ETH_PHY_10BASET_FD_NOAUTO
// ARM_ETH_PHY_100BASET_HD_NOAUTO
// ARM_ETH_PHY_100BASET_FD_NOAUTO
// ARM_ETH_PHY_100BASET_HD_AUTO
// ARM_ETH_PHY_REPEATER
// ARM_ETH_PHY_LOW_POWER
// ARM_ETH_PHY_FULL_AUTO
void eth_init (eth_t *u, const char *name, int prio, struct _mem_pool_t *pool,
               struct _arp_t *arp, const uint8_t *macaddr, uint8_t phy_mode);

//
// Инициализация выводов под светодиоды Ethernet (указаны в target.cfg)
void eth_led_init(void);

//
// Вывод отладочной информации по драйверу Ethernet
//
//  u       указатель на структуру, описывающую интерфейс
//  stream  указатель на поток, в который выдавать отладочную
//          информацию
//
void eth_debug (eth_t *u, struct _stream_t *stream);

//
// Ожидание завершения автоподстройки
//
void eth_waiting_autonegotiation (void);

//
// Выполнить определение параметров сети
//
//  u       указатель на структуру, описывающую интерфейс
//
void eth_restart_autonegotiation (eth_t *u);

//
// Определить наличие несущей в линии
//
//  u       указатель на структуру, описывающую интерфейс
//
uint16_t eth_get_carrier (eth_t *u);

//
// Определить текущие настройки скорости
//
//  u       указатель на структуру, описывающую интерфейс
//  duplex  указатель на переменную, в которую будет помещен
//          признак дуплексного режима
//  возвращает скорость в бит/сек
uint32_t eth_get_speed (eth_t *u, uint8_t *duplex);

//
// Установить внутреннюю петлю в PHY для самотестирования устройства
//
//  u       указатель на структуру, описывающую интерфейс
//  on      1 - установить петлю, 0 - отключить петлю
//
void eth_set_phy_loop (eth_t *u, uint8_t on);

//
// Установить внутреннюю петлю в MAC для самотестирования устройства
//
//  u       указатель на структуру, описывающую интерфейс
//  on      1 - установить петлю, 0 - отключить петлю
//
void eth_set_mac_loop (eth_t *u, uint8_t on);

//
// Установить параметры режима прослушивания сети
//
//  u       указатель на структуру, описывающую интерфейс
//  station узел
//  group   группа
void eth_set_promisc (eth_t *u, uint8_t station, uint8_t group);

//
// Опросить устройство
//
//  u       указатель на структуру, описывающую интерфейс
//
void eth_poll (eth_t *u);


#ifdef __cplusplus
}
#endif

#endif
