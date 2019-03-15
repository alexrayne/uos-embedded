// Для включения старого драйвера (Вакуленко) добавить  
// ARM_1986BE1_OLD_ETH в target.cfg
//#define TCP_SERVER
#define TCP_CLIENT
//#define UDP_SERVER
//#define UDP_CLIENT

#include <runtime/lib.h>
#include <kernel/uos.h>
#include <kernel/internal.h>
#include <timer/timer.h>
#if (ARM_1986BE1_OLD_ETH && defined(TCP_SERVER))
#   include <milandr/eth.h>
#else
#   include <milandr/ve1eth.h>
#endif
#include <stream/stream.h>
#include <mem/mem.h>
#include <net/route.h>
#include <net/ip.h>
#include <net/arp.h>
#include <buf/buf.h>

task_t *console_task, *test_task;
extern task_t *eth_task;
mem_pool_t pool;
arp_t *arp;
route_t route;
eth_t eth_data, *eth = &eth_data;
ip_t ip;
timer_t timer;

unsigned count;
unsigned old_count;
unsigned errors = 0;

//======================================================================
//**************** Testing TCP protocol: TCP_SERVER ********************
//======================================================================

#ifdef TCP_SERVER
#include <net/tcp.h>

ARRAY (stack_con, 800); 
ARRAY (stack_tcp, 1000); 
#if !(ARM_1986BE1_OLD_ETH)
ARRAY (stack_drv, ETH_STACKSZ);
#endif 
ARRAY (group, sizeof(mutex_group_t) + 4 * sizeof(mutex_slot_t));
ARRAY (arp_data, sizeof(arp_t) + 10 * sizeof(arp_entry_t));

tcp_socket_t *user_socket;

#define PORT_IP         48059
#define MSG_SIZE        375
uint32_t msg[MSG_SIZE];
uint8_t tcp_disconnect=0;

void tcp_task (void *data)
{
    tcp_socket_t *lsock;
    int sz;
    uint16_t i;
    uint32_t *pMSG, cycle __attribute__((unused));
    uint16_t blink        __attribute__((unused));
    
    cycle = blink = 0;
    pMSG = &msg[0];
    lsock = tcp_listen (&ip, 0, PORT_IP);
    
    if(! lsock) {
        printf (&debug, "\nError on listen, aborted\n");
        uos_halt (0);
    }
    for(;;) {
        printf (&debug, "\nTCP Server waiting on port %d...\n", PORT_IP);
        printf (&debug, "Free memory: %d bytes\n", mem_available (&pool));
        user_socket = tcp_accept (lsock);
        if(! user_socket) {
            printf (&debug, "\nError on accept\n");
            uos_halt (0);
        }
        debug_printf ("\nClient connected\n");
        count = 0;
        old_count = 0;
        for(;;) {
            sz = tcp_read (user_socket, msg, sizeof(msg));
            if((sz < 0) || tcp_disconnect)
                break;
        
/*                          TCP Duplicate Acknowledgement
    When a sender sends a segment, information is also sent about the sequence number used. 
    The receiver sends an acknowledgement(ACK) with the ACK flag set. This tells the sender 
    that the receiver received that segment. Note that for TCP segment there is a retransmission 
    timer bound to it. If the sender doesn’t receive an ACK from the receiver for the TCP segment 
    sent before the timer expire then the sender retransmits the same TCP segment.
    It may also happen sometimes that a receiver receives a TCP segment with a sequence number 
    higher than the expected one. Then the receiver sends an immediate ACK with the relevant 
    field set to the Sequence number the receiver was expecting. Note that this ACK is duplicate 
    of an ACK which was previously sent. The reason to do this is to update the sender with regards 
    to the dropped/missing TCP segments. Once, 2 DUP ACKS(Dupilcate Acknowledgements), TCP performs 
    a retransmission of that segment without waiting for the expiry of the retransmission timer. 
    This is known as Fast Retransmit. */

            if(cycle % 5 == 0)
                tcp_ack_now (user_socket); // Immediate ACK
            cycle++;

            sz >>= 2;
            for(i = 0; i < sz; ++i)  {
                if (pMSG[i] != count) {
                    debug_printf("\nbad counter: %d, expected: %d\n", pMSG[i], count);
                    count = pMSG[i] + 1;
                    errors++;
                    continue;
                }
                count++; 
            }
            #if !(ARM_1986BE1_OLD_ETH)
            blink++;
            if(blink % 500 == 0)  
                eth_led ();
            #endif
        }
        tcp_close (user_socket);
        mem_free (user_socket);
        user_socket = 0;  
        tcp_disconnect=0;
        debug_printf ("\nTCP close\n\n");
    }
}

void console (void *unused)
{
    uint8_t st_con   __attribute__((unused));
    char c           __attribute__((unused));
    buf_t *p         __attribute__((unused));
    uint32_t start   __attribute__((unused));
    uint32_t end     __attribute__((unused));
    uint32_t elapsed __attribute__((unused));
    uint32_t bytes   __attribute__((unused));

    st_con = 0;
    start = timer_milliseconds (&timer);
    for (;;) {
        timer_delay (&timer, 1000);
        end = timer_milliseconds (&timer);
        elapsed = end - start;
        bytes = (count - old_count) << 2;
        old_count = count;
        start = end;
    
        debug_printf("rcv rate: %llu (bytes/sec), errors: %u\n",  (bytes * 1000 / elapsed), errors);
        //debug_printf(" %3d: pool %d - tcp_task_stack %d - eth_task_stack %d - speed_in %d B/s - speed_out %d B/s - STATUS %08X    \r", st_con++, mem_available (&pool),
        //             task_stack_avail(test_task),task_stack_avail(eth_task), eth->netif.in_bytes, eth->netif.out_bytes, ARM_ETH->STAT);
        eth->netif.in_bytes = eth->netif.out_bytes = 0;
    
        //task_set_priority (console_task, 120);
        
        if(peekchar(&debug) >= 0) {
            c = getchar(&debug);
            if(c == 'r' || c == 'R')  {
                //dbg_tcp_dump();
            } else if (c == 'd' || c == 'D') {
                tcp_disconnect = 1;
            } else if (c == 't' || c == 'T') {
                debug_printf("\n");
                task_print (&debug, 0);
                task_print (&debug, (task_t*) stack_con);
                task_print (&debug, (task_t*) stack_tcp);
                #if (ARM_1986BE1_OLD_ETH)
                task_print (&debug, (task_t*) eth->stack);
                #else
                task_print (&debug, (task_t*) stack_drv);
                #endif
                task_print (&debug, (task_t*) ip.stack);
                printf (&debug, "mem(pool) %d\n", mem_available (&pool));
                putchar (&debug, '\n');
            } 
        }
        //task_set_priority (console_task, 10);
    }
}

#define DATA_HI_POOL
void uos_init(void)
{
#ifdef DATA_HI_POOL
    // Динамическая память в data_hi
    extern unsigned __hi_data_end[], _hstack[];
    memset((void *)0x20100000, 0, 16*1024);
    mem_init(&pool, (unsigned) __hi_data_end, (unsigned) _hstack );
#else
    // Динамическая память в SRAM
    extern unsigned long __bss_end, _estack;
    mem_init(&pool, (unsigned) &__bss_end, (unsigned) &_estack - 256);
#endif

    // системный таймер
    timer_init (&timer, KHZ, 50);

    // Create a group of two locks: timer and eth. 
    mutex_group_t *g = mutex_group_init (group, sizeof(group));
    mutex_group_add(g, &eth->netif.lock);
    mutex_group_add(g, &timer.decisec);

    arp = arp_init(arp_data, sizeof(arp_data), &ip);
    ip_init(&ip, &pool, 70, &timer, arp, g);
    
    uint8_t my_macaddr[] = {0x01,0xBC,0x3D,0x17,0xA0,0x11};
#if (ARM_1986BE1_OLD_ETH)
    eth_init (eth, "eth0", 80, &pool, arp, my_macaddr);
    debug_printf ("eth_driver: old driver\n");
#else
    // Инициализация обработчика прерываний 
    create_eth_interrupt_task(eth, 100, stack_drv, sizeof(stack_drv)); 
    // Create interface eth0 
    eth_init(eth, "eth0", 80, &pool, arp, my_macaddr, ARM_ETH_PHY_FULL_AUTO);
    debug_printf ("eth_driver: ve1eth\n");
#endif
    debug_printf("TCP server, IRQ mode  \n");

    uint8_t my_ip[] = { 192, 168, 30, 5 };
    route_add_netif(&ip, &route, my_ip, 24, &eth->netif);

    test_task = task_create(tcp_task, 0, "tcp_srv", 20, stack_tcp, sizeof(stack_tcp));
    console_task = task_create(console, 0, "con", 25, stack_con, sizeof(stack_con)); 
   
    eth_led_init ();
   //eth_restart_autonegotiation(eth);
}

#endif //TCP_SERVER

//======================================================================
//**************** Testing TCP protocol: TCP_CLIENT ********************
//======================================================================

#ifdef TCP_CLIENT
#include <net/tcp.h>

ARRAY (stack_con, 500);
ARRAY (stack_tcp, 1000);
ARRAY (stack_drv, 1000);

ARRAY (group, sizeof(mutex_group_t) + 4 * sizeof(mutex_slot_t));
ARRAY (arp_data, sizeof(arp_t) + 10 * sizeof(arp_entry_t));

tcp_socket_t *user_socket;
task_t *console_task;

#define PORT_IP     0xBBBB
#define BUF_SIZE    365
int buf[BUF_SIZE];
unsigned char server_ip[] = { 192, 168, 30, 6 };
uint8_t tcp_disconnect=0;

void tcp_task (void *data)
{
    int i, sz;
    unsigned blink=0;
    for(;;) {
        debug_printf ("\nPress ENTER to connect to %d.%d.%d.%d:%d\n", server_ip[0],
                      server_ip[1], server_ip[2], server_ip[3], PORT_IP);
        debug_getchar ();

        user_socket = tcp_connect (&ip, server_ip, PORT_IP);
        if(! user_socket) {
            debug_printf ("Failed to connect!\n");
            continue;
        }
        debug_printf ("\nConnected to server\n");
        count = 0;
        old_count = 0;

        for(;;) {

            for(i = 0; i < BUF_SIZE; ++i) 
                buf[i] = count++;

            /* send a message to the server PORT on machine HOST */
            sz = tcp_write (user_socket, buf, sizeof(buf));
            if(sz < 0) {
                debug_printf ("Disconnected\n");
                break;
            }
        
            if(count % 1000000 == 0)
                mdelay (1);
            
            blink++;
            if(blink % 80 == 0)
                eth_led ();
        }
        tcp_close (user_socket);
        mem_free (user_socket);
        user_socket = 0;
    }
}

void console (void *unused)
{
    uint32_t start      __attribute__((unused));
    uint32_t end        __attribute__((unused));
    uint32_t elapsed    __attribute__((unused));
    uint32_t bytes      __attribute__((unused));
    uint8_t st_con      __attribute__((unused));
    start = timer_milliseconds (&timer);
    st_con = 0;
    char c;

    for (;;) {
        timer_delay (&timer, 1000);

        end = timer_milliseconds (&timer);
        elapsed = end - start;
        bytes = (count - old_count) << 2;
        old_count = count;
        start = end;
        //debug_printf ("snd rate: %d (bytes/sec) counter %d\n", bytes * 1000 / elapsed);
        debug_printf (" %3d: speed_in %d B/s - speed_out %d B/s - STATUS %08X   \r",st_con++,eth->netif.in_bytes,eth->netif.out_bytes, ARM_ETH->STAT);
        eth->netif.in_bytes = eth->netif.out_bytes = 0;
    
        if(peekchar(&debug) >= 0) { 
            c = getchar(&debug);
            if(c == 'r' || c == 'R') {
                //dbg_tcp_dump();
            } else if (c == 'd' || c == 'D') {
                tcp_disconnect = 1;
            } else if (c == 't' || c == 'T') {
                debug_printf("\n");
                task_print (&debug, 0);
                task_print (&debug, (task_t*) stack_con);
                task_print (&debug, (task_t*) stack_tcp);
                task_print (&debug, (task_t*) stack_drv);
                task_print (&debug, (task_t*) ip.stack);
                printf (&debug, "mem(pool) %d\n", mem_available (&pool));
                putchar (&debug, '\n');
            }
        }
    }
}

void uos_init (void)
{
    // Динамическая память в SRAM 
    extern unsigned long __bss_end, _estack;
    mem_init(&pool, (unsigned) &__bss_end, (unsigned) &_estack - 256);

    // системный таймер
    timer_init (&timer, KHZ, 50);

    // Create a group of two locks: timer and eth. 
    mutex_group_t *g = mutex_group_init (group, sizeof(group));
    mutex_group_add(g, &eth->netif.lock);
    mutex_group_add(g, &timer.decisec);

    arp = arp_init(arp_data, sizeof(arp_data), &ip);
    ip_init(&ip, &pool, 70, &timer, arp, g);

    // Инициализация обработчика прерываний 
    create_eth_interrupt_task(eth, 100, stack_drv, sizeof(stack_drv));
    debug_printf("TCP client, IRQ mode  \n");
    
    // Create interface eth0
    unsigned char my_macaddr[] = {0x01,0xBC,0x3D,0x17,0xA0,0x11};
    eth_init(eth, "eth0", 80, &pool, arp, my_macaddr, ARM_ETH_PHY_FULL_AUTO);

    unsigned char my_ip[] = { 192, 168, 30, 5 };
    route_add_netif(&ip, &route, my_ip, 24, &eth->netif);

    task_create(tcp_task, 0, "tcp", 20, stack_tcp, sizeof(stack_tcp));
    console_task = task_create(console, 0, "con", 25, stack_con, sizeof(stack_con));
}
#endif //TCP_CLIENT

//======================================================================
//**************** Testing UDP protocol: UDP_SERVER ********************
//======================================================================

#ifdef UDP_SERVER
#include <net/udp.h>

ARRAY (stack_con, 500);
ARRAY (stack_udp, 1000);
ARRAY (stack_drv, 1000);
ARRAY (group, sizeof(mutex_group_t) + 4 * sizeof(mutex_slot_t));
ARRAY (arp_data, sizeof(arp_t) + 10 * sizeof(arp_entry_t));

udp_socket_t sock;
task_t *console_task;

#define UDP_PORT        0xBBBB

void udp_task (void *data)
{
    int sz;
    unsigned i;
    unsigned char addr [4];
    unsigned short port;
    buf_t *p;
    unsigned *msg;
    unsigned blink=0;

    udp_socket (&sock, &ip, UDP_PORT);
    printf (&debug, "\nUDP Server waiting on port %d...\n", UDP_PORT);
    printf (&debug, "Free memory: %d bytes\n", mem_available (&pool));

    for(;;) {
        p = udp_recvfrom (&sock, addr, &port);
        
        if(!p) continue;
    
        sz = (p->tot_len >> 2);
        msg = (unsigned *) p->payload;

        for(i = 0; i < sz; ++i) {
            if(msg[i] != count) {
                //debug_printf("bad counter: %d, expected: %d\n", msg[i], count);
                count = msg[i] + 1;
                errors++;
                continue;
            }
            ++count;
        }
        buf_free (p);
        blink++;
        if(blink % 200 == 0)  eth_led ();
    }
}

void console (void *unused)
{
    uint32_t old_count = 0;
    uint32_t start, end, elapsed;
    uint32_t bytes;
    uint8_t st_con=0;

    start = timer_milliseconds (&timer);

    for(;;) {
        timer_delay (&timer, 1000);
        end = timer_milliseconds (&timer);
        elapsed = end - start;
        bytes = (count - old_count) << 2;
        old_count = count;
        start = end;
        task_set_priority (console_task, 1);
        debug_printf (" %3d: rcv rate %lu (bytes/sec), errors: %u\r", st_con++, (unsigned) (bytes * 1000 / elapsed), errors);
        task_set_priority (console_task, 100);
    }
}

void uos_init (void)
{
    // Динамическая память в SRAM 
    extern unsigned long __bss_end, _estack;
    mem_init(&pool, (unsigned) &__bss_end, (unsigned) &_estack - 256);

    // системный таймер
    timer_init (&timer, KHZ, 50);
    
    // Create a group of two locks: timer and eth. 
    mutex_group_t *g = mutex_group_init (group, sizeof(group));
    mutex_group_add(g, &eth->netif.lock);
    mutex_group_add(g, &timer.decisec);

    arp = arp_init(arp_data, sizeof(arp_data), &ip);
    ip_init(&ip, &pool, 70, &timer, arp, g);

    // Инициализация обработчика прерываний 
    create_eth_interrupt_task(eth, 100, stack_drv, sizeof(stack_drv));
    debug_printf("UDP server, IRQ mode  \n");
    
    // Create interface eth0
    unsigned char my_macaddr[] = {0x01,0xBC,0x3D,0x17,0xA0,0x11};
    eth_init(eth, "eth0", 80, &pool, arp, my_macaddr, ARM_ETH_PHY_FULL_AUTO);
    
    unsigned char my_ip[] = { 192, 168, 30, 5 };
    route_add_netif(&ip, &route, my_ip, 24, &eth->netif);

    task_create (udp_task, 0, "udp", 65, stack_udp, sizeof (stack_udp));
    console_task = task_create (console, 0, "con", 1, stack_con, sizeof (stack_con));
}
#endif

//======================================================================
//**************** Testing UDP protocol: UDP_CLIENT ********************
//======================================================================
#ifdef UDP_CLIENT
#include <net/udp.h>

ARRAY (stack_udp, 1000);
ARRAY (stack_drv, 1000);
ARRAY (group, sizeof(mutex_group_t) + 4 * sizeof(mutex_slot_t));
ARRAY (arp_data, sizeof(arp_t) + 10 * sizeof(arp_entry_t));

udp_socket_t sock;

#define UDP_PORT        0xBBBB
#define BUF_SIZE        365
uint32_t buf [BUF_SIZE];
unsigned char server_ip[] = { 192, 168, 30, 6 };

void udp_task (void *data)
{
    int i;
    buf_t *p;
    uint32_t start;
    uint32_t old_count __attribute__((unused));
    uint32_t end       __attribute__((unused));
    uint32_t elapsed   __attribute__((unused));
    uint32_t bytes     __attribute__((unused));
    uint16_t blink=0;
    uint8_t st_con=0;

    old_count=0;
    start = timer_milliseconds (&timer);

    debug_printf ("\nPress ENTER to start sending to %d.%d.%d.%d:%d\n", server_ip[0],
                  server_ip[1], server_ip[2], server_ip[3], UDP_PORT);
    debug_getchar ();

    udp_socket (&sock, &ip, UDP_PORT);
    for (;;) {
        p = buf_alloc (&pool, BUF_SIZE * 4, 42);

        for (i = 0; i < BUF_SIZE; ++i) 
            buf[i] = count++;
        memcpy (p->payload, buf, sizeof(buf));

        udp_sendto (&sock, p, server_ip, UDP_PORT);
    
        if (timer_passed (&timer, start, 1000)) {
            end = timer_milliseconds (&timer);
            elapsed = end - start;
            bytes = (count - old_count) << 2;
            old_count = count;
            //debug_printf ("snd rate: %lu (bytes/sec)\n", (unsigned) (bytes * 1000 / elapsed));
            start = end;
            debug_printf (" %3d: speed_in %d B/s - speed_out %d B/s - STATUS %08X   \r",st_con++,eth->netif.in_bytes,eth->netif.out_bytes, ARM_ETH->STAT);
            eth->netif.in_bytes = eth->netif.out_bytes = 0;
        }
        blink++;
        if(blink % 100 == 0)  
            eth_led ();
    }
}

void uos_init (void)
{
    // Динамическая память в SRAM 
    extern unsigned long __bss_end, _estack;
    mem_init(&pool, (unsigned) &__bss_end, (unsigned) &_estack - 256);

    // системный таймер
    timer_init (&timer, KHZ, 50);
    
    // Create a group of two locks: timer and eth. 
    mutex_group_t *g = mutex_group_init (group, sizeof(group));
    mutex_group_add(g, &eth->netif.lock);
    mutex_group_add(g, &timer.decisec);

    arp = arp_init(arp_data, sizeof(arp_data), &ip);
    ip_init(&ip, &pool, 70, &timer, arp, g);
    
    // Инициализация обработчика прерываний 
    create_eth_interrupt_task(eth, 100, stack_drv, sizeof(stack_drv));
    debug_printf("UDP client, IRQ mode  \n");
    
    // Create interface eth0
    unsigned char my_macaddr[] = {0x01,0xBC,0x3D,0x17,0xA0,0x11};
    eth_init(eth, "eth0", 80, &pool, arp, my_macaddr, ARM_ETH_PHY_FULL_AUTO);
    
    unsigned char my_ip[] = { 192, 168, 30, 5 };
    route_add_netif(&ip, &route, my_ip, 24, &eth->netif);

    task_create (udp_task, 0, "udp", 65, stack_udp, sizeof (stack_udp));
}
#endif
