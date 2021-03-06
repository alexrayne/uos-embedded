/*
 * Testing SNMP protocol.
 */
#include <runtime/lib.h>
#include <kernel/uos.h>
#include <mem/mem.h>
#include <buf/buf.h>
#include <net/route.h>
#include <net/ip.h>
#include <net/udp.h>
#include <snmp/snmp.h>
#include <timer/timer.h>
#include <tap/tap.h>

#define MEM_SIZE		15000
#define STATIC_STRING(val)	({static char s[] = val; s; })

ARRAY (task, 6000);
ARRAY (group, sizeof(mutex_group_t) + 4 * sizeof(mutex_slot_t));
char memory [MEM_SIZE];
mem_pool_t pool;
tap_t tap;
route_t route;
route_t default_route;
timer_t timer;
ip_t ip;
snmp_t snmp;
udp_socket_t sock;

/*
 * Declare get/getnext/set functions.
 */
#include <snmp/snmp-var.h>
#include <snmp/snmp-system.h>
#include <snmp/snmp-netif.h>
#include <snmp/snmp-ip.h>
#include <snmp/snmp-snmp.h>
#include <snmp/snmp-icmp.h>
#include <snmp/snmp-udp.h>

#include <snmp/snmp-vardecl.h>
SYSTEM_VARIABLE_LIST
IF_VARIABLE_LIST
IP_VARIABLE_LIST
ICMP_VARIABLE_LIST
/*TCP_VARIABLE_LIST*/
UDP_VARIABLE_LIST
SNMP_VARIABLE_LIST

const snmp_var_t snmp_tab [] = {
#include <snmp/snmp-vardef.h>
	SYSTEM_VARIABLE_LIST
	IF_VARIABLE_LIST
	IP_VARIABLE_LIST
	ICMP_VARIABLE_LIST
/*	TCP_VARIABLE_LIST*/
	UDP_VARIABLE_LIST
	SNMP_VARIABLE_LIST
};

void main_task (void *data)
{
	buf_t *p, *r;
	unsigned char addr [4], *output;
	unsigned short port;

	udp_socket (&sock, &ip, 161);
	debug_printf ("test_snmp: mem available = %d bytes\n",
		mem_available (&pool));

	for (;;) {
		p = udp_recvfrom (&sock, addr, &port);
		debug_printf ("test_snmp: mem available = %d bytes\n",
			mem_available (&pool));

		r = buf_alloc (&pool, 1500, 50);
		if (! r) {
			debug_printf ("test_snmp: out of memory!\n");
			buf_free (p);
			continue;
		}

		output = snmp_execute (&snmp, p->payload, p->len,
			r->payload, r->len);
		buf_free (p);
		if (! output) {
			buf_free (r);
			continue;
		}

		buf_add_header (r, - (output - r->payload));
		udp_sendto (&sock, r, addr, port);
	}
}

void uos_init (void)
{
	mutex_group_t *g;
	unsigned char my_ip[] = "\310\0\0\2";
	unsigned char gw_ip[] = "\310\0\0\1";
	unsigned char gw_netmask[] = "\0\0\0\0";

	timer_init (&timer, KHZ, 10);
	mem_init (&pool, (size_t) memory, (size_t) memory + MEM_SIZE);

	/*
	 * Create a group of two locks: timer and tap.
	 */
	g = mutex_group_init (group, sizeof(group));
	mutex_group_add (g, &tap.netif.lock);
	mutex_group_add (g, &timer.decisec);
	ip_init (&ip, &pool, 70, &timer, 0, g);

	/*
	 * Create interface tap0 200.0.0.2 / 255.255.255.0
	 */
	tap_init (&tap, "tap0", 80, &pool, 0);
	route_add_netif (&ip, &route, my_ip, 24, &tap.netif);

	/*
	 * Add default route to 200.0.0.1
	 */
	route_add_gateway (&ip, &default_route, gw_netmask, 0, gw_ip);
	if (! default_route.netif)
		debug_printf ("test_snmp: no interface for default route!\n");

	snmp_init (&snmp, &pool, &ip, snmp_tab, sizeof(snmp_tab), 20520,
		SNMP_SERVICE_REPEATER, STATIC_STRING("Testing SNMP"),
		"1.3.6.1.4.1.20520.1.1", "Test", "1.3.6.1.4.1.20520.6.1");

	task_create (main_task, 0, "main", 1, task, sizeof (task));
}
