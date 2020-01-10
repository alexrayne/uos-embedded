#include <runtime/lib.h>
#include <kernel/uos.h>
#include <timer/timer.h>
#include <buf/buf.h>
#include <mem/mem.h>
#include <crc/crc16-inet.h>
#include <net/netif.h>
#include <net/route.h>
#include <net/ip.h>
#include <net/udp.h>
#include <net/tcp.h>
#include <net/arp.h>

const unsigned long IP_ZERO_ADDR = 0;
const unsigned long IP_BROADCAST_ADDR = 0xFFFFFFFF;

#ifdef DEBUG_NET_IP
#   ifndef IP_PRINTF
#define IP_printf(...) debug_printf(__VA_ARGS__)
#   else
#define IP_printf(...) IP_PRINTF(__VA_ARGS__)
#   endif
#else
#define IP_printf(...)
#endif

/*
 * Forward an IP packet. It finds an appropriate route for the packet,
 * decrements the TTL value of the packet, adjusts the checksum and outputs
 * the packet on the appropriate interface.
 */
static void
ip_forward (ip_t *ip, buf_t *p, ip_addr_const gateway, netif_t *netif,
        ip_addr_const netif_ipaddr)
{
	ip_hdr_t *iphdr = (ip_hdr_t*) p->payload;

	/* Decrement TTL and send ICMP if ttl == 0. */
	if (iphdr->ttl <= 1) {
		/* Don't send ICMP messages in response to ICMP messages */
		if (iphdr->proto != IP_PROTO_ICMP) {
			icmp_time_exceeded (ip, p);
		}
		return;
	}
	iphdr->ttl--;

	/* Incremental update of the IP checksum. */
	if (iphdr->chksum_h != 0xff)
		iphdr->chksum_h += 1;
	else if (iphdr->chksum_l != 0xff) {
		iphdr->chksum_h = 0;
		iphdr->chksum_l += 1;
	} else {
		iphdr->chksum_h = 1;
		iphdr->chksum_l = 0;
	}

	/* Forwarding packet to netif. */
	if (! gateway)
		gateway = iphdr->dest.var;
	netif_output (netif, p, gateway, netif_ipaddr);
	++ip->forw_datagrams;
}

/*
 * This function is called by the network interface device driver
 * when an IP packet is received. The function does the basic checks
 * of the IP header such as packet size being at least larger than
 * the header size etc. If the packet was not destined for us, the packet
 * is forwarded (using ip_forward). The IP checksum is always checked.
 * Finally, the packet is sent to the upper layer protocol input function.
 */
void
ip_input (ip_t *ip, buf_t *p, netif_t *inp)
{
	ip_hdr_t *iphdr = (ip_hdr_t*) p->payload;
	unsigned char hlen, broadcast;

	++ip->in_receives;

	/* Identify the IP header */
	if ((iphdr->version & 0xf0) != 0x40) {
		/* Bad version number. */
	    IP_printf ("ip_input: bad version number %d\n", iphdr->version >> 4);
		buf_free (p);
		++ip->in_hdr_errors;
		return;
	}

	hlen = (iphdr->version & 0x0f) * 4;
	if (hlen > p->len) {
		/* Too short packet. */
	    IP_printf ("ip_input: too short packet (hlen=%d bytes)\n", hlen);
		buf_free (p);
		++ip->in_hdr_errors;
		return;
	}

	/* Verify checksum */
	if (crc16_inet (0, p->payload, hlen) != CRC16_INET_GOOD) {
		/* Failing checksum. */
	    IP_printf ("ip_input: bad checksum\n", hlen);
		buf_free (p);
		++ip->in_hdr_errors;
		return;
	}

	/* Trim buf. This should have been done at the netif layer,
	 * but we'll do it anyway just to be sure that its done. */
    {
        unsigned req_len = iphdr->len_h << 8 | iphdr->len_l;

        if ( (netif_is_overlaped(p) != 0)           //overlaped buffers should not be reallocated
             || ((p->tot_len - req_len) < req_len)  //TCP_CAPLEN
           )
            buf_truncate_soft (p, req_len);
        else {
            buf_truncate (p, req_len);
        }
    }

	IP_printf("ip:have packet : %@.4D ->%@.4D len %d proto %x\n"
	            ,iphdr->src.ucs, iphdr->dest.ucs, p->len
	            , iphdr->proto
	          );
	
	/* Is this packet for us? */
	broadcast = ipadr_is_broadcast(iphdr->dest.var);
	if (! broadcast) {
		netif_t *netif;

		netif = route_lookup_self (ip, iphdr->dest.var, &broadcast);
		if (! netif) {
			/* Packet not for us, route or discard */
			if (ip->forwarding && ! broadcast) {
			    ip_addr_const netif_ipaddr = 0;
			    ip_addr_const gateway = 0;

				netif = route_lookup (ip, iphdr->dest.var, &gateway, &netif_ipaddr);
				if (! ipadr_not0(gateway))
					gateway = iphdr->dest.var;

				/* Don't forward packets onto the same
				 * network interface on which they arrived. */
				if (netif && netif != inp)
					ip_forward (ip, p, gateway, netif, netif_ipaddr);
				else
					buf_free (p);
			} else {
				++ip->in_addr_errors;
				/*debug_printf ("ip_input: no route to host %d.%d.%d.%d\n",
					iphdr->dest[0], iphdr->dest[1], iphdr->dest[2], iphdr->dest[3]);*/
				buf_free (p);
			}
			return;
		}
	}
	if (broadcast)
		++inp->in_mcast_pkts;

	if (iphdr->offset_l || (iphdr->offset_h & (IP_OFFMASK | IP_MF)) != 0) {
		/* Drop fragmented packets. */
		/*debug_printf ("ip_input: fragmentation not implemented\n");*/
		buf_free (p);
		++ip->in_hdr_errors;
		return;
	}

	if (hlen > IP_HLEN) {
		/* Drop packets with IP options. */
		/*debug_printf ("ip_input: options not implemented\n");*/
		buf_free (p);
		++ip->in_hdr_errors;
		return;
	}

	/* Send to upper layers */
	++ip->in_delivers;
	switch (iphdr->proto) {
	case IP_PROTO_UDP:
		buf_add_header (p, -IP_HLEN);
		p = udp_input (ip, p, inp, iphdr);
		if (p) {
			/* No match was found, send ICMP destination
			 * port unreachable unless destination address
			 * was broadcast/multicast. */
			if (! broadcast && ! IS_MULTICAST (iphdr->dest.ucs)) {
				buf_add_header (p, IP_HLEN);
				icmp_dest_unreach (ip, p, ICMP_DUR_PORT);
			} else
				buf_free (p);
		}
		break;

	case IP_PROTO_TCP:
		if (! tcp_input)
			goto proto_unreach;
		buf_add_header (p, -IP_HLEN);
		tcp_input (ip, p, inp, iphdr);
		break;

	case IP_PROTO_ICMP:
		++ip->icmp_in_msgs;
		switch (p->payload [IP_HLEN]) {
		case ICMP_ECHO:
			/* Ignore ICMP messages on broadcasts */
			++ip->icmp_in_echos;
			if (! broadcast && ! IS_MULTICAST (iphdr->dest.ucs)){
				icmp_echo_request (ip, p, inp);
			}
			else
				buf_free (p);
			break;
		default:
			/* ICMP type not supported. */
			/*debug_printf ("ip_input: unsupported ICMP op\n");*/
			++ip->icmp_in_errors;
			buf_free (p);
			break;
		}
		break;

	default:
proto_unreach:
		/*debug_printf ("ip_input: protocol not implemented\n");*/

		/* Send ICMP destination protocol unreachable
		 * unless is was a broadcast */
		if (! broadcast && ! IS_MULTICAST (iphdr->dest.ucs)) {
			icmp_dest_unreach (ip, p, ICMP_DUR_PROTO);
		} else
			buf_free (p);

		/* Unsupported transport protocol. */
		--ip->in_delivers;
		++ip->in_unknown_protos;
		++inp->in_unknown_protos;
		break;
	}
}

bool_t
ip_add_header (ip_t *ip, buf_t *p
        , ip_addr_const dest, ip_addr_const src
        , small_uint_t proto)
{
	ip_hdr_t *iphdr;
	unsigned short chksum;
    //mutex_t* iplock = iptx_lock_ensure(ip);

	++ip->out_requests;
	if (! buf_add_header (p, IP_HLEN)) {
		/* Not enough room for IP header. */
		/*debug_printf ("ip_output_netif: no space for header\n");*/
		++ip->out_discards;
		//iptx_unlock_ensure(iplock);
		return 0;
	}

	/*
	 * Build an IP header.
	 */
	iphdr = (ip_hdr_t*) p->payload;
	iphdr->ttl = ip->default_ttl;
	iphdr->proto = proto;
	iphdr->version = 0x40 + IP_HLEN / 4;
	iphdr->len_h = p->tot_len >> 8;
	iphdr->len_l = p->tot_len;
	iphdr->tos = ip->tos;
	iphdr->offset_h = 0;
	iphdr->offset_l = 0;
	++ip->id;
	iphdr->id_h = ip->id >> 8;
	iphdr->id_l = ip->id;

	//iptx_unlock_ensure(iplock);

	iphdr->dest.val = dest;
    iphdr->src.val = src;

	iphdr->chksum_h = 0;
	iphdr->chksum_l = 0;
	chksum = ~crc16_inet (0, p->payload, IP_HLEN);
#if HTONS(1) == 1
	iphdr->chksum_h = chksum >> 8;
	iphdr->chksum_l = chksum;
#else
	iphdr->chksum_h = chksum;
	iphdr->chksum_l = chksum >> 8;
#endif

	/*buf_print_ip (p);*/
	return 1;
}

/*
 * Send an IP packet on a network interface. This function constructs
 * the IP header and calculates the IP header checksum.
 * dest     - destination IP address
 * src      - source IP adress. If NULL, the IP address of outgoing
 *        interface is used instead
 * proto    - protocol field value
 * gateway  - IP address of gateway
 * netif    - outgoing interface. If NULL, the routing table is searched
 *        for an interface, using destination address as a key
 * netif_ipaddr - IP address of outgoing interface (when netif is not NULL)
 */
bool_t
ip_output_netif (ip_t *ip, buf_t *p
        , ip_addr_const dest, ip_addr_const src
        , small_uint_t proto
        , ip_addr_const gateway
        , netif_t *netif , ip_addr_const netif_ipaddr)
{
    if (!src)
        src = netif_ipaddr;
    if (!ip_add_header(ip, p, dest, src, proto)){
        netif_free_buf (netif, p);
        return 0;
    }
	if (!ipadr_not0(gateway))
		gateway = dest;
    IP_printf ("ip: netif %s output %d bytes to %@.4D, gate %@.4D\n"
            ,netif->name, p->tot_len
            , ipref_as_ucs(dest), ipref_as_ucs(gateway)
            );

	return netif_output (netif, p, gateway, netif_ipaddr);
}

/*
 * Send an IP packet.
 */
bool_t
ip_output2(ip_t *ip, struct _buf_t *p
        , ip_addr_const dest, ip_addr_const src
        , small_uint_t proto)
{
	netif_t *netif;
    ip_addr_const netif_ipaddr;
    ip_addr_const gateway;

	/* Find the outgoing network interface. */
	netif = route_lookup (ip, dest, &gateway, &netif_ipaddr);
	if (! netif) {
		/* No route to host. */
	    IP_printf("ip_output: no route to host %@.4D\n", ipref_as_ucs(dest));
	    //mutex_t* iplock = iptx_lock_ensure(ip);
		++ip->out_requests;
		++ip->out_no_routes;
		//iptx_unlock_ensure(iplock);
		netif_free_buf (0, p);
		return 0;
	}
	return ip_output_netif (ip, p, dest, src, proto
	        , gateway, netif, netif_ipaddr
	        );
}

unsigned iph_stamp(ip_t *ip, netif_t* u){
    unsigned now = ip->route_stamp;
    if (ip->arp)
        now += ip->arp->stamp;
    if (u->arp)
        now += u->arp->stamp;
    return now;
}

bool_t ip_hcache_is_fresh(ip_t *ip, ip_header_cache* iph)
{
    if (iph->tot_len < IP_CAPLEN)
        return 0;
    netif_io_overlap* over = netif_is_overlaped(iph);
    if (over == 0)
        return 0;
    netif_t* u = (netif_t*)(over->arg);
    if (u == 0)
        return 0; 
    unsigned now = iph_stamp(ip, u);
    unsigned stamp = over->arg2;
    return (stamp == now)? 1: 0;
}

//* \arg p - буффер данных, должен иметь IP_HRESERVE резервированого места для заголовка
//* \arg iph - буффер с образцовым заголовком ip, ранее закешированным.
//*             если заголовок устарел, используются адресаты и идентификаторы протокола из него 
//*             чтобы построить новый 
bool_t ip_output_withheader(ip_t *ip, buf_t *p, ip_header_cache* iph)
{
    assert(iph != 0);
    //this is minimal header len
    assert(iph->tot_len >= IP_CAPLEN);

    if (ip_hcache_is_fresh(ip, iph)) {
        netif_io_overlap* over = netif_is_overlaped(iph);
        netif_t* netif = (netif_t*)(over->arg);
        ip_hdr_t* iphdr = (ip_hdr_t*) (p->payload - IP_HLEN);
        //IP_printf ("ip out cached:...");
    
        unsigned ipframe_len = p->tot_len + IP_HLEN;

        if (! buf_add_header (p, iph->tot_len)) {
            /* Not enough room for IP header. */
            /*debug_printf ("ip_output_netif: no space for header\n");*/
            //mutex_t* iplock = iptx_lock_ensure(ip);
            ++ip->out_requests;
            ++ip->out_discards;
            //iptx_unlock_ensure(iplock);
            netif_free_buf (netif, p);
            return 0;
        }
        memcpy(p->payload, iph->payload, iph->tot_len);

        //mutex_t* iplock = iptx_lock_ensure(ip);
        ++ip->out_requests;
        /*
         * Refresh an IP header.
         */
        iphdr->ttl = ip->default_ttl;
        iphdr->len_h = ipframe_len >> 8;
        iphdr->len_l = ipframe_len;
        ++ip->id;
        iphdr->id_h = ip->id >> 8;
        iphdr->id_l = ip->id;
        //iptx_unlock_ensure(iplock);
    
        iphdr->chksum_h = 0;
        iphdr->chksum_l = 0;
        unsigned short chksum;
        chksum = ~crc16_inet (0, iphdr, IP_HLEN);
    #if HTONS(1) == 1
        iphdr->chksum_h = chksum >> 8;
        iphdr->chksum_l = chksum;
    #else
        iphdr->chksum_h = chksum;
        iphdr->chksum_l = chksum >> 8;
    #endif
        IP_printf ("ip: netif %s output %d bytes to %@.4D, cached\n"
                ,netif->name, p->tot_len
                , iphdr->dest.ucs
                );
        netif_io_overlap* overp = netif_is_overlaped(p);
        if (overp)
            overp->status |= nios_IPOK;
        return netif->interface->output (netif, p, 0);
    }
    else{ //if (ip_hcache_is_fresh(ip, iph))
        ip_hdr_t* iphdr = (ip_hdr_t*) (iph->payload + iph->tot_len - IP_HLEN);
        return ip_output2(ip, p, iphdr->dest.val, iphdr->src.val, iphdr->proto);
    }
}

bool_t ip_refresh_hcache(ip_t *ip, struct _netif_t *netif
                        , ip_header_cache* iph, buf_t *p, unsigned hlen
                        )
{
    if (ip_hcache_is_fresh(ip, iph))
        return 1;

    //IP_printf ("ip hcache: assign header cache[%d] ... ", hlen);

    //just reset cache to clean state
    buf_add_header(iph, -iph->tot_len);

    ip_hdr_t* iphdr = (ip_hdr_t*)(p->payload+hlen-IP_HLEN);
    if (netif == 0){
        /* Find the outgoing network interface. */
        netif = route_lookup (ip, iphdr->dest.var, 0, 0);
        if (netif == 0){
            return 0;
        }
    }
    IP_printf ("ip hcache: %S -> %@.4D\n", netif->name, iphdr->dest.ucs);

    if(!buf_add_header(iph, hlen))
        return 0;
    memcpy(iph->payload, p->payload, hlen);

    netif_io_overlap* over = netif_overlap_init(iph);
    over->arg = (unsigned)netif;
    unsigned now = iph_stamp(ip, netif);
    over->arg2 = now;
    return 1;
}

/*
 * Listener task. Get a lock group as an argument.
 * Waits for signals on the group, and prints them.
 */
static void
ip_main (void *arg)
{
	ip_t *ip = (ip_t*) arg;
	mutex_t *m;
	netif_t *netif;
	buf_t *p;

	assert (ip->netif_group->num > 0);
	mutex_group_listen (ip->netif_group);
	for (;;) {
		mutex_group_wait (ip->netif_group, &m, 0);
		mutex_lock (&ip->lock);
		if (ip->timer && m == &ip->timer->decisec) {
			/* Per 0.1 second timer interrupt. */
			/*debug_printf ("ip: timer %d\n", timer_milliseconds (ip->timer));*/
/*debug_printf ("[%d] ", timer_milliseconds (ip->timer));*/
			if (ip->arp)
				arp_timer (ip->arp);

			++ip->tcp_timer;
            const unsigned char time_limit = (TCP_SLOW_INTERVAL*2)/TCP_TMR_INTERVAL;
			if (ip->tcp_timer >= time_limit)
				ip->tcp_timer = 0;

			/* Call tcp_fasttmr() every 200 ms. */
            const unsigned char time_fast = (TCP_FAST_INTERVAL/TCP_TMR_INTERVAL);
			if (tcp_fasttmr && ( (ip->tcp_timer%time_fast + 1) == time_fast))
				tcp_fasttmr (ip);

			/* Call tcp_slowtmr() every 500 ms. */
            const unsigned char time_slow = (TCP_SLOW_INTERVAL/TCP_TMR_INTERVAL);
			if (tcp_slowtmr &&
			    (ip->tcp_timer == 0 || ip->tcp_timer == time_slow))
				tcp_slowtmr (ip);
		} else {
			/* Interrupt from driver. */
			netif = (netif_t*) m;
			//IP_printf ("ip: netif %s\n", netif->name);

			for (;;) {
				p = netif_input (netif);
				if (! p)
					break;
				/*debug_printf ("ip: netif %S received %d bytes\n",
					netif->name, p->tot_len);*/
				/*buf_print_ip (p);*/
				ip_input (ip, p, netif);
			}
		}
		mutex_unlock (&ip->lock);
	}
}

/*
 * Initialize the IP layer.
 * TODO - хочется отвязаться от использования клока, перейти на таймаут или сигналящий мутех.
 */
void
ip_init (ip_t *ip, mem_pool_t *pool, int prio,
	timer_t *timer, arp_t *arp, mutex_group_t *g)
{
	ip->pool = pool;
	ip->timer = timer;
	ip->arp = arp;
	ip->netif_group = g;
	ip->default_ttl = 64;

	/* Initialize the TCP layer. */
	ip->tcp_seqno = 6510;
	ip->tcp_port = TCP_LOCAL_PORT_RANGE_START;

    mutex_init(&ip->lock);
#if IP_LOCK_STYLE >= IP_LOCK_STYLE_DEPOUT
    mutex_init(&ip->lock_tx);
#endif

	task_create (ip_main, ip, "ipv4", prio, ip->stack, sizeof (ip->stack));
}
