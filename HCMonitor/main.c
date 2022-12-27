/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>


#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_string_fns.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>

#include "config.h"
#include "monitor.h"

char blog[10] = "burst.txt";
char rlog[20] = "response.txt";

#define RTE_LOGTYPE_L2FWD RTE_LOGTYPE_USER1

#define NB_MBUF   8192
#define NB_SOCKETS 2
#define RTE_RING_SZ 8192


#define MAX_PKT_BURST 1024
#define MAX_FREE 16
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */

/*#define RX_LCORE 0
#define PCAP_LCORE (RX_LCORE+MAX_QUE_NUM)
#define MONITOR_LCORE (PCAP_LCORE+PR_NUM)
#define TIMER_LCORE (MONITOR_LCORE+1)
#define CDF_LCORE (TIMER_LCORE+1)
#define TSTAMP_LCORE (CDF_LCORE+1)
#define SSD_LCORE (TSTAMP_LCORE+1)*/

int RX_LCORE; 
int PCAP_LCORE; 
int MONITOR_LCORE; 
int TIMER_LCORE; 
int CDF_LCORE;
int TSTAMP_LCORE; 
int SSD_LCORE; 

/*
 * Configurable number of RX/TX ring descriptors
 */
struct rte_ring *output_ring[RTE_MAX_ETHPORTS];

struct tik_mbuf{
//#ifdef PMD_MODE
	struct rte_mbuf *m;
/*
#else
	struct rte_ipv4_hdr *iph;
	uint16_t len;
#endif
*/
	struct timespec now;
};

struct ssd_queue{
    struct tik_mbuf *ring;
    volatile unsigned long occupy;
    uint8_t empty[64];
    volatile unsigned long deque;	
};

typedef struct ssd_queue* ssd_t;
ssd_t *SSD_Ring;

#define RTE_TEST_RX_DESC_DEFAULT 2048
#define RTE_TEST_TX_DESC_DEFAULT 512
static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;


/* ethernet addresses of ports */
static struct rte_ether_addr l2fwd_ports_eth_addr[RTE_MAX_ETHPORTS];

/* mask of enabled ports */
static uint32_t l2fwd_enabled_port_mask = 0;

/* list of enabled ports */
static uint32_t l2fwd_dst_ports[RTE_MAX_ETHPORTS];

static unsigned int l2fwd_rx_queue_per_lcore = 1;

extern unsigned long max_size;

uint8_t nr_ports = 0;

unsigned tx_len = 0;

struct mbuf_table {
	unsigned len;
	struct rte_mbuf *m_table[RTE_RING_SZ];
	struct rte_ring **output_ring;
};

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16
struct lcore_queue_conf {
	unsigned n_rx_port;
	unsigned n_rx_queue;
	unsigned rx_port_list[MAX_RX_QUEUE_PER_LCORE];
	struct mbuf_table tx_mbufs[RTE_MAX_ETHPORTS];

} __rte_cache_aligned;
struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];

#define ETH_RSS_I40E (\
		    ETH_RSS_FRAG_IPV4 | \
		    ETH_RSS_NONFRAG_IPV4_TCP | \
		    ETH_RSS_NONFRAG_IPV4_UDP | \
		    ETH_RSS_NONFRAG_IPV4_SCTP | \
		    ETH_RSS_NONFRAG_IPV4_OTHER | \
		    ETH_RSS_FRAG_IPV6 | \
		    ETH_RSS_NONFRAG_IPV6_TCP | \
		    ETH_RSS_NONFRAG_IPV6_UDP | \
		    ETH_RSS_NONFRAG_IPV6_SCTP | \
		    ETH_RSS_NONFRAG_IPV6_OTHER | \
		    ETH_RSS_L2_PAYLOAD)

#define ETH_RSS_E1000_IGB (\
		    ETH_RSS_IPV4 | \
		    ETH_RSS_NONFRAG_IPV4_TCP| \
		    ETH_RSS_NONFRAG_IPV4_UDP| \
		    ETH_RSS_IPV6 | \
		    ETH_RSS_NONFRAG_IPV6_TCP | \
		    ETH_RSS_NONFRAG_IPV6_UDP | \
		    ETH_RSS_IPV6_EX | \
		    ETH_RSS_IPV6_TCP_EX | \
		    ETH_RSS_IPV6_UDP_EX)


static const struct rte_eth_conf port_conf = {
    .rxmode = {
        .mq_mode    = ETH_MQ_RX_RSS,
        .max_rx_pkt_len = RTE_ETHER_MAX_LEN,
        .split_hdr_size = 0,
        .offloads = DEV_RX_OFFLOAD_CHECKSUM,
    },
    .rx_adv_conf = {
        .rss_conf = {
            .rss_key = NULL,
			.rss_hf = ETH_RSS_I40E,
            //.rss_hf = ETH_RSS_E1000_IGB,
        },
    },
    .txmode = {
        .mq_mode = ETH_MQ_TX_NONE,
    },
};

struct rte_mempool * l2fwd_pktmbuf_pool[MAX_QUE_NUM];

/* Per-port statistics struct */
struct l2fwd_port_statistics {
	uint64_t tx;
	uint64_t rx;
	uint64_t dropped;
	uint64_t freed;
	/*for period*/
	uint64_t tx_per;
	uint64_t rx_per;
	uint64_t rx_last;
	uint64_t dropped_per;
	uint64_t freed_per;
	/*for monitor*/
	uint64_t tcp_psh;
} __rte_cache_aligned;
struct l2fwd_port_statistics port_statistics[RTE_MAX_ETHPORTS];

unsigned queue_states[MAX_QUE_NUM];
unsigned pkt_free[MAX_QUE_NUM];

/* A tsc-based timer responsible for triggering statistics printout */
#define TIMER_MILLISECOND 2200000ULL /* around 1ms at 2 Ghz */
#define MAX_TIMER_PERIOD 86400 /* 1 day max */
static int64_t timer_period = 5 * TIMER_MILLISECOND * 1; /* default period is 10 ms */
static int64_t timer_stper = 1 * TIMER_MILLISECOND * 1000; /* default period is 1 seconds */

/* Print out statistics on packets dropped */
static void
print_stats(void)
{
	uint64_t total_packets_dropped, total_packets_freed,total_packets_tx, total_packets_rx;
	uint64_t total_tcp_psh;
	unsigned i,portid,total_que_rx = 0,total_pkt_free = 0;
	struct rte_eth_stats eth_stats;

	total_packets_dropped = 0;
	total_packets_freed = 0;
	total_packets_tx = 0;
	total_packets_rx = 0;

	total_tcp_psh = 0;

	const char clr[] = { 27, '[', '2', 'J', '\0' };
	const char topLeft[] = { 27, '[', '1', ';', '1', 'H','\0' };


		/* Clear screen and move to top left */
	printf("%s%s", clr, topLeft);

	printf("\nPort statistics ====================================");


	for (portid = 0; portid < nr_ports; portid++) {
		/* skip disabled ports */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;
		
		rte_eth_stats_get(portid, &eth_stats);

		for(i = 0; i < conf->rx_que; i++){
			total_que_rx += queue_states[i];
			total_pkt_free += pkt_free[i];
		}
	
		printf("\nStatistics for port %u ------------------------------"
			   "\nPackets sent: %24"PRIu64
			   "\nPackets received: %20"PRIu64
			   "\nPackets dropped: %22"PRIu64
			   "\nPackets in_err: %22"PRIu64
			   "\nPackets Mbuf_err: %20"PRIu64
			   "\n-------- Statistics for monitor --------"
			   "\nPackets tcp: %25"PRIu64
			   "\nPackets in Tx Buffer: %16d"
			   "\nPackets freed: %23"PRIu64
			   "\nPackets dropped: %21"PRIu64,
			   portid,
			   port_statistics[portid].tx,
			   total_que_rx,//port_statistics[portid].rx,
			   eth_stats.imissed,
			   eth_stats.ierrors,
			   eth_stats.rx_nombuf,
			   port_statistics[portid].tcp_psh,
			   tx_len,
			   total_pkt_free,//port_statistics[portid].freed,
			   port_statistics[portid].dropped);

		total_packets_dropped += port_statistics[portid].dropped;
		total_packets_freed += port_statistics[portid].freed;
		total_tcp_psh += port_statistics[portid].tcp_psh;
		total_packets_tx += port_statistics[portid].tx;
		total_packets_rx += total_que_rx;//port_statistics[portid].rx;
	}
	printf("\nAggregate statistics ==============================="
		   "\nTotal packets sent: %18"PRIu64
		   "\nTotal packets received: %14"PRIu64
		   "\nTotal packets tcp: %19"PRIu64
		   "\nTotal packets freed: %17"PRIu64
		   "\nTotal packets dropped: %15"PRIu64,
		   total_packets_tx,
		   total_packets_rx,
		   total_tcp_psh,
		   total_packets_freed,
		   total_packets_dropped);
	printf("\n====================================================\n");

}

/* Send the burst of packets on an output interface */
#if 0
static int
l2fwd_send_burst(struct lcore_queue_conf *qconf, unsigned n, uint8_t port)
{
	struct rte_mbuf **m_table;

	m_table = (struct rte_mbuf **)qconf->tx_mbufs[port].m_table;

#ifdef MIRROR
	unsigned i = 0;
	do {
		rte_pktmbuf_free(m_table[i]);
	} while (++i < n);

	port_statistics[port].freed += n;
#else
	unsigned ret;
	unsigned queueid =0;
	ret = rte_eth_tx_burst(port, (uint16_t) queueid, m_table, (uint16_t) n);
	port_statistics[port].tx += ret;
	if (unlikely(ret < n)) {
		port_statistics[port].dropped += (n - ret);
		do {
			rte_pktmbuf_free(m_table[ret]);
		} while (++ret < n);
	}
#endif

	return 0;
}
#endif

#if 0
/* Read packets for TX queue and prepare them to be freed */
static int
l2fwd_free_packet(uint8_t port,struct rte_mbuf **m_table)
{
	unsigned i = 0;
	/* enough pkts to be sent */
	if (tx_len > 0) {
		do {
			rte_pktmbuf_free(m_table[i]);
		} while (++i < tx_len);

		port_statistics[port].freed += tx_len;
		tx_len = 0;
	}
	return 0;
}
#endif

/* Enqueue packets for TX and prepare them to be sent */
#if 0
static int
l2fwd_send_packet(struct rte_mbuf *m, uint8_t port)
{

#ifdef MIRROR
	unsigned len;
	

	unsigned lcore_id;
	lcore_id = rte_lcore_id();

	struct lcore_queue_conf *qconf;

	qconf = &lcore_queue_conf[lcore_id];
	len = qconf->tx_mbufs[port].len;
	qconf->tx_mbufs[port].m_table[len] = m;
	len++;
	tx_len++;
	
#if 0
	/* enough pkts to be sent */
	if (unlikely(len == MAX_PKT_BURST)) {
		l2fwd_send_burst(qconf, MAX_PKT_BURST, port);
		len = 0;
	}
#endif

	qconf->tx_mbufs[port].len = len;
#else
	rte_pktmbuf_free(m);
	port_statistics[port].freed += 1;
#endif
	//printf("send packet lcore_id:%d,port:%d,len:%d,tx_len:%d\n",lcore_id,port,len,tx_len);
	
	return 0;
}
#endif

static void
l2fwd_simple_forward(struct rte_mbuf *m, unsigned portid, struct timespec ts_now, int lcore_id)
{
	struct rte_ether_hdr *eth;
    struct rte_tcp_hdr *tcp;
	
	eth = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
	
#ifdef MIRROR

	//if(likely(RTE_ETH_IS_IPV4_HDR(m->packet_type)))
    {
    	struct rte_ipv4_hdr *ip_hdr;      
        if(rte_cpu_to_be_16(eth->ether_type) == RTE_ETHER_TYPE_VLAN)
        {
        	uint16_t vlanoff = sizeof(struct rte_vlan_hdr) + sizeof(struct rte_ether_hdr);
        	ip_hdr = rte_pktmbuf_mtod_offset(m, struct rte_ipv4_hdr *,
						   vlanoff);
        }
        else
            ip_hdr = rte_pktmbuf_mtod_offset(m, struct rte_ipv4_hdr *,
						   sizeof(struct rte_ether_hdr));

        tcp = (struct rte_tcp_hdr *)((unsigned char *) ip_hdr + sizeof(struct rte_ipv4_hdr));
		uint16_t total_len = rte_be_to_cpu_16(ip_hdr->total_length);
		uint16_t payload_len = total_len - sizeof(struct rte_ipv4_hdr) 
										- ((tcp->data_off & 0xf0) >> 2);
	    if (likely((uint8_t)ip_hdr->next_proto_id == 6 && payload_len > 0 ))
	    {
			port_statistics[portid].tcp_psh++;

    		if(!(packet_process(ip_hdr, tcp, ts_now, lcore_id, payload_len)))
				err++;
    			//printf("packet process failed!\n");
		}
	}
	rte_pktmbuf_free(m);
	//l2fwd_send_packet(m, (uint8_t) dst_port);
	//port_statistics[portid].freed += 1;
	pkt_free[lcore_id] +=1;
		
#else
	unsigned dst_port;

	dst_port = l2fwd_dst_ports[portid];
	
	void *tmp;

	/* 02:00:00:00:00:xx */
	tmp = &eth->d_addr.addr_bytes[0];
	*((uint64_t *)tmp) = 0x000000000002 + ((uint64_t)dst_port << 40);

	/* src addr */
	ether_addr_copy(&l2fwd_ports_eth_addr[dst_port], &eth->s_addr);

	l2fwd_send_packet(m, (uint8_t) dst_port);
#endif
}

/* main processing loop */
static void
l2fwd_main_loop(void)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	struct rte_mbuf *m; 
	struct tik_mbuf *ssd;
	unsigned lcore_id;
	uint64_t prev_tsc, diff_tsc, cur_tsc, timer_tsc;
	uint64_t prev_sta,diff_sta,cur_sta,timer_sta;
	unsigned i, j, q, portid, nb_rx;
	struct lcore_queue_conf *qconf;
	struct timespec ts_now;
	
	struct rte_ether_hdr *eth;
    struct rte_tcp_hdr *tcp;
    struct rte_ipv4_hdr *ip_hdr;      
	
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S * BURST_TX_DRAIN_US;

	prev_tsc = 0;
	timer_tsc = 0;
	prev_sta = 0;
	timer_sta = 0;

	lcore_id = rte_lcore_id();
	
	qconf = &lcore_queue_conf[lcore_id];

	if(lcore_id >= conf->rx_que && lcore_id < MONITOR_LCORE){
	    int ret, dive;
		int idx, pos, end;
		idx = lcore_id - PCAP_LCORE;
		dive = conf->rx_que / conf->pr_que;
		pos = idx * dive;
		end = (idx + 1) * dive;
	    while(1){
        	for(i = pos; i < end; i++){
				if((SSD_Ring[i]) && (SSD_Ring[i]->occupy != SSD_Ring[i]->deque)){
					struct tik_mbuf *tm = SSD_Ring[i]->ring + SSD_Ring[i]->deque;

#ifdef PMD_MODE
					ret = trans_pcap(tm->m, tm->now, i);
#else
					eth = rte_pktmbuf_mtod(tm->m, struct rte_ether_hdr *);
        			if(rte_cpu_to_be_16(eth->ether_type) == RTE_ETHER_TYPE_VLAN)
        			{
        				uint16_t vlanoff = sizeof(struct rte_vlan_hdr) 
										+ sizeof(struct rte_ether_hdr);
        				ip_hdr = rte_pktmbuf_mtod_offset(tm->m, struct rte_ipv4_hdr *,
						   								vlanoff);
        			}else
            			ip_hdr = rte_pktmbuf_mtod_offset(tm->m, struct rte_ipv4_hdr *,
						  		 sizeof(struct rte_ether_hdr));

        			tcp = (struct rte_tcp_hdr *)((unsigned char *) ip_hdr + sizeof(struct rte_ipv4_hdr));
					uint16_t total_len = rte_be_to_cpu_16(ip_hdr->total_length);
					uint16_t payload_len = total_len - sizeof(struct rte_ipv4_hdr) 
													- ((tcp->data_off & 0xf0) >> 2);
	    			if (likely((uint8_t)ip_hdr->next_proto_id == 6 && payload_len > 0 )){
						port_statistics[0].tcp_psh++;

    					if(!(packet_process(ip_hdr, tcp, tm->now, i, payload_len))){
							err++;
    					//printf("packet process failed!\n");
						}
					}
#endif
                	SSD_Ring[i]->deque = (SSD_Ring[i]->deque + 1) % conf->buffer_len;
            	}

			}
	    }
	}

#ifdef PMD_MODE	
	if(lcore_id == SSD_LCORE){
	    int ret;
        pcap_header ph;
        while(1){
        	for(i = 0; i < conf->rx_que; i++){
		    	if(PP_Ring[i]->occupy != PP_Ring[i]->deque){
			    	pcap_t p = PP_Ring[i]->ring[PP_Ring[i]->deque];
		    	    ret = fwrite(&p->ph, sizeof(pcap_header), 1, fp_out);
			    	ret = fwrite(p->buff, 1, p->ph.capture_len, fp_out);	
                    PP_Ring[i]->deque = (PP_Ring[i]->deque + 1) % conf->buffer_len;
                }

			}
	    }
    }
#endif

	if(lcore_id == MONITOR_LCORE){
        int time = 0;
        int i = 0;
		while(1){
			int label = 0;
            time++;
            if(time == 1000){
                rte_delay_us(1);
                time = 0;
            }
            for(i = 0; i < conf->rx_que; i++){
			    if(PrQue[i]->occupy != PrQue[i]->deque){	
                	node_t q = PrQue[i]->RxQue + PrQue[i]->deque;
		    		label = response_time_process(q, pkt_num, 0);
		    		if(unlikely(label == -2))
    				{
    					miss_rep++;	
    				}
    				PrQue[i]->deque = (PrQue[i]->deque + 1) % max_size;
			    }
            }
		}
	}

	if(lcore_id == TSTAMP_LCORE){
		RTE_LOG(INFO, L2FWD, "entering log loop on lcore %u\n", lcore_id);
		while(1){	
			clock_gettime(CLOCK_MONOTONIC_RAW, &ts); 
			cur_sta = rte_rdtsc();
			/*
			 * TX burst queue drain
			 */
			diff_sta = cur_sta - prev_sta;	
			/* if timer is enabled */
			if (timer_stper > 0) {
			   
					/* advance the timer */
					timer_sta += diff_sta;
			
					/* if timer has reached its timeout */
					if (likely(timer_sta >= (uint64_t) timer_stper)) {
							print_stats();
							for(q = 0;q < conf->rx_que; q++){
								printf("queue %d rx %d pkts.\n",q, queue_states[q]);
								if(SSD_Ring[q]){
							    	printf("\noccupy:%ld,deque:%ld,pkt_req:%d,pkt_rep:%d\n",
                                    /*PrQue[q]->occupy,PrQue[q]->deque,pkt_req - prev_req, pkt_rep - prev_rep);*/

                                    SSD_Ring[q]->occupy,SSD_Ring[q]->deque,pkt_req - prev_req, pkt_rep - prev_rep);
								}
							}
							/* reset the timer */
							timer_sta = 0;
							prev_req = pkt_req;
							prev_rep = pkt_rep;
					}
				}
			
			prev_sta = cur_sta;
		}
	}
			

	if(lcore_id == TIMER_LCORE){
	    RTE_LOG(INFO, L2FWD, "entering timer loop on lcore %u\n", lcore_id);
        FILE *fp_r = fopen(rlog,"a");
        int i;
		while(1){
		
			cur_tsc = rte_rdtsc();
			
			/*
			 * TX burst queue drain
			 */
			diff_tsc = cur_tsc - prev_tsc;
			if (unlikely(diff_tsc > drain_tsc)) {
			
				/* if timer is enabled */
				if (timer_period > 0) {
			   
					/* advance the timer */
					timer_tsc += diff_tsc;
			
					/* if timer has reached its timeout */
					if (unlikely(timer_tsc >= (uint64_t) timer_period)) {
							/* reset the timer */
							timer_tsc = 0;
                            if(conf->enable_mcc){
							//uint64_t start_t = rte_rdtsc();
	    						FILE *fp = fopen(blog, "a");			
								for(i = 0; i < IP_NUM; i++){
								fprintf(fp,"%s:",ip_src[i]);//,start_t);
								fprintf(fp,"%d,",burst[i]);//,start_t);
                                burst[i] = 0;
                            	}
								fprintf(fp,"%d\n",request_num);//,start_t);
								fclose(fp);
                            	request_num = 0;
                            }
					}
				}
			
				prev_tsc = cur_tsc;
			}
		}
	}

	if(lcore_id == CDF_LCORE){
		lcore_online();
	}
	


	if (qconf->n_rx_port == 0) {
		RTE_LOG(INFO, L2FWD, "\nlcore %u has nothing to do\n", lcore_id);
		return;
	}

	RTE_LOG(INFO, L2FWD, "entering main loop on lcore %u\n", lcore_id);
	
	SSD_Ring[lcore_id] = (ssd_t)calloc(1, sizeof(struct ssd_queue));
    SSD_Ring[lcore_id]->deque = SSD_Ring[lcore_id]->occupy = 0;
    SSD_Ring[lcore_id]->ring = (struct tik_mbuf*)calloc(conf->buffer_len, sizeof(struct tik_mbuf));
    struct rte_mbuf* tmp = (struct rte_mbuf*)calloc(conf->buffer_len, sizeof(struct rte_mbuf));
	for(j = 0; j < conf->buffer_len; j++){
//#ifdef PMD_MODE
         SSD_Ring[lcore_id]->ring[j].m = tmp + j;
    }

	for (i = 0; i < qconf->n_rx_port; i++) {

		portid = qconf->rx_port_list[i];
		RTE_LOG(INFO, L2FWD, " -- lcoreid=%u portid=%u\n", lcore_id,
			portid);
	}

	while (1) {
		/*
		 * Read packet from RX queues
		 */
		for (i = 0; i < qconf->n_rx_port; i++) {
			portid = qconf->rx_port_list[i];
			nb_rx = rte_eth_rx_burst((uint8_t) portid, lcore_id,
						 	pkts_burst, MAX_PKT_BURST);
			//port_statistics[portid].rx += nb_rx;
			queue_states[lcore_id] += nb_rx; 
			/*if(nb_rx)
				printf("rx %d pkts from lcore_id %d port %d\n"
					"port_statistics rx:%d\n",nb_rx,lcore_id,portid,port_statistics[portid].rx);
			*/
			for (j = 0; j < nb_rx; j++) {
				m = pkts_burst[j];
				rte_prefetch0(rte_pktmbuf_mtod(m, void *));
				if(m->pkt_len > 70){
					if(likely((SSD_Ring[lcore_id]->occupy + 1) % conf->buffer_len != SSD_Ring[lcore_id]->deque)){
						ssd = SSD_Ring[lcore_id]->ring + SSD_Ring[lcore_id]->occupy;
						memcpy(ssd->m, m, sizeof(struct rte_mbuf));
						ssd->now.tv_sec = ts.tv_sec;
						ssd->now.tv_nsec = ts.tv_nsec;
						_mm_sfence();
						SSD_Ring[lcore_id]->occupy = (SSD_Ring[lcore_id]->occupy + 1) % conf->buffer_len;
					}else{
						printf("EXCP:SSD buffer is Full!\n");
					}
				}
				rte_pktmbuf_free(m);
        		pkt_free[lcore_id] +=1;
			}
		}
	}
}


static int
l2fwd_launch_one_lcore(__attribute__((unused)) void *dummy)
{
	uint64_t hz = rte_get_tsc_hz();
	printf("cpu hz: %llu\n", hz);
	l2fwd_main_loop();
	return 0;
}

/* display usage */
static void
l2fwd_usage(const char *prgname)
{
	printf("%s [EAL options] -- -p PORTMASK [-q NQ]\n"
	       "  -p PORTMASK: hexadecimal bitmask of ports to configure\n"
	       "  -q NQ: number of queue (=ports) per lcore (default is 1)\n"
		   "  -T PERIOD: statistics will be refreshed each PERIOD seconds (0 to disable, 10 default, 86400 maximum)\n",
	       prgname);
}

static int
l2fwd_parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	/* parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if (pm == 0)
		return -1;

	return pm;
}

static unsigned int
l2fwd_parse_nqueue(const char *q_arg)
{
	char *end = NULL;
	unsigned long n;

	/* parse hexadecimal string */
	n = strtoul(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;
	if (n == 0)
		return 0;
	if (n >= MAX_RX_QUEUE_PER_LCORE)
		return 0;

	return n;
}

static int
l2fwd_parse_timer_period(const char *q_arg)
{
	char *end = NULL;
	int n;

	/* parse number string */
	n = strtol(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;
	if (n >= MAX_TIMER_PERIOD)
		return -1;

	return n;
}

/* Parse the argument given in the command line of the application */
static int
l2fwd_parse_args(int argc, char **argv)
{
	int opt, ret;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];
	static struct option lgopts[] = {
		{NULL, 0, 0, 0}
	};

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, "p:q:T:",
				  lgopts, &option_index)) != EOF) {

		switch (opt) {
		/* portmask */
		case 'p':
			l2fwd_enabled_port_mask = l2fwd_parse_portmask(optarg);
			if (l2fwd_enabled_port_mask == 0) {
				printf("invalid portmask\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;

		/* nqueue */
		case 'q':
			l2fwd_rx_queue_per_lcore = l2fwd_parse_nqueue(optarg);
			if (l2fwd_rx_queue_per_lcore == 0) {
				printf("invalid queue number\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;

		/* timer period */
		case 'T':
			timer_period = l2fwd_parse_timer_period(optarg) * 1000 * TIMER_MILLISECOND;
			if (timer_period < 0) {
				printf("invalid timer period\n");
				l2fwd_usage(prgname);
				return -1;
			}
			break;

		/* long options */
		case 0:
			l2fwd_usage(prgname);
			return -1;

		default:
			l2fwd_usage(prgname);
			return -1;
		}
	}

	if (optind >= 0)
		argv[optind-1] = prgname;

	ret = optind-1;
	optind = 0; /* reset getopt lib */
	return ret;
}


/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint8_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint8_t portid, count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(portid, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					printf("Port %d Link Up - speed %u "
						"Mbps - %s\n", (uint8_t)portid,
						(unsigned)link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					("full-duplex") : ("half-duplex\n"));
				else
					printf("Port %d Link Down\n",
						(uint8_t)portid);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == 0) {
				all_ports_up = 0;
				break;
			}
		}
		/* after finally printing all link status, get out */
		if (print_flag == 1)
			break;

		if (all_ports_up == 0) {
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			printf("done\n");
		}
	}
}


int
main(int argc, char **argv)
{
	struct lcore_queue_conf *qconf;
	struct rte_eth_dev_info dev_info;
	int q,ret;
	uint8_t nb_ports;
	uint8_t nb_ports_available;
	uint8_t portid, last_port;
	unsigned lcore_id, rx_lcore_id, socket_id;
	unsigned nb_ports_in_mask = 0;
	char s[64];
	
	conf = initConfig();
    getConfig(conf);

	const uint16_t rx_queue = conf->rx_que;

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	argc -= ret;
	argv += ret;

	/* parse application arguments (after the EAL ones) */
	ret = l2fwd_parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid L2FWD arguments\n");

	/* create the mbuf pool */
	for(q = 0; q < rx_queue; q++){
		if(l2fwd_pktmbuf_pool[q] == NULL){
			snprintf(s, sizeof(s), "mbuf_pool_%d", q);
			l2fwd_pktmbuf_pool[q] = rte_pktmbuf_pool_create(s, NB_MBUF, 32,
				0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
			if (l2fwd_pktmbuf_pool[q] == NULL)
				rte_exit(EXIT_FAILURE, "Cannot init mbuf pool on lcore %d,socket %d\n"
						,q,rte_socket_id());
		}
	}

	nb_ports = rte_eth_dev_count_avail();
	nr_ports = nb_ports;
	
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

	if (nb_ports > RTE_MAX_ETHPORTS)
		nb_ports = RTE_MAX_ETHPORTS;

	/* reset l2fwd_dst_ports */
	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++)
		l2fwd_dst_ports[portid] = 0;
	last_port = 0;

	/*
	 * Each logical core is assigned a dedicated TX queue on each port.
	 */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;

		if (nb_ports_in_mask % 2) {
			l2fwd_dst_ports[portid] = last_port;
			l2fwd_dst_ports[last_port] = portid;
		}
		else
			last_port = portid;

		nb_ports_in_mask++;

		rte_eth_dev_info_get(portid, &dev_info);
	}
	if (nb_ports_in_mask % 2) {
		printf("Notice: odd number of ports in portmask.\n");
		l2fwd_dst_ports[last_port] = last_port;
	}

	rx_lcore_id = 0;
	qconf = NULL;

	/* Initialize the port/queue configuration of each logical core */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0)
			continue;

		/* Assigned a new logical core in the loop above. */
		for(q = 0; q < rx_queue; q++){
			if (qconf != &lcore_queue_conf[q])
				qconf = &lcore_queue_conf[q];
			qconf->rx_port_list[qconf->n_rx_port] = portid;
			qconf->n_rx_queue = 1;
			qconf->n_rx_port = 1;
			printf("Lcore %u: RX port %u\n", rx_lcore_id, (unsigned) portid);
		}
	}

	nb_ports_available = nb_ports;

	/* Initialise each port */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((l2fwd_enabled_port_mask & (1 << portid)) == 0) {
			printf("Skipping disabled port %u\n", (unsigned) portid);
			nb_ports_available--;
			continue;
		}
		/* init port */
		printf("Initializing port %u... ", (unsigned) portid);
		fflush(stdout);
		ret = rte_eth_dev_configure(portid, rx_queue, 1, &port_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n",
				  ret, (unsigned) portid);

		rte_eth_macaddr_get(portid,&l2fwd_ports_eth_addr[portid]);

		/* init one RX queue */
		fflush(stdout);
		for(q = 0;q < rx_queue; q++){
			/* get the lcore_id for this port */
			if(rte_lcore_is_enabled(q) == 0 ) {
				rte_exit(EXIT_FAILURE, "Cannot start Rx thread on lcore %u:"
					"lcore disabled\n",lcore_id);
			}
			ret = rte_eth_rx_queue_setup(portid, q, nb_rxd,
					     rte_eth_dev_socket_id(portid),
					     NULL,
					     l2fwd_pktmbuf_pool[q]);
			if (ret < 0)
				rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, queue=%d, port=%u\n",
				  	ret, q, (unsigned) portid);
		}

		/* init one TX queue on each port */
		fflush(stdout);
		ret = rte_eth_tx_queue_setup(portid, 0, nb_txd,
				rte_eth_dev_socket_id(portid),
				NULL);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n",
				ret, (unsigned) portid);

		/* Start device */
		ret = rte_eth_dev_start(portid);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n",
				  ret, (unsigned) portid);

		printf("done: \n");

		rte_eth_promiscuous_enable(portid);

		printf("Port %u, MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
				(unsigned) portid,
				l2fwd_ports_eth_addr[portid].addr_bytes[0],
				l2fwd_ports_eth_addr[portid].addr_bytes[1],
				l2fwd_ports_eth_addr[portid].addr_bytes[2],
				l2fwd_ports_eth_addr[portid].addr_bytes[3],
				l2fwd_ports_eth_addr[portid].addr_bytes[4],
				l2fwd_ports_eth_addr[portid].addr_bytes[5]);

		/* initialize port stats */
		memset(&port_statistics, 0, sizeof(port_statistics));
	}

	if (!nb_ports_available) {
		rte_exit(EXIT_FAILURE,
			"All available ports are disabled. Please set portmask.\n");
	}

	/*
	* scheduler ring is read only by the transmitter core, but written to
	* by multiple threads
	*/			
	for (portid = 0; portid < nb_ports; portid ++)
	{
		char name[32];
		snprintf(name, sizeof(name), "Output_ring_%u", portid);
		output_ring[portid] = rte_ring_create(name, RTE_RING_SZ * 2048,
		rte_socket_id(), RING_F_SC_DEQ);
		if (output_ring[portid] == NULL)
			rte_exit(EXIT_FAILURE, "Cannot create output ring\n");
		
		rte_ring_dump(stdout, output_ring[portid]);
	}


		/* initialize Rx Packet Key Queue */
#ifdef MIRROR
		InitQueue();
		res_setup_hash(0);
		Qcur = KeyQue;
#endif

#ifdef OP_MYSQL
		init_mysql();
#endif

//#else
//		SSD_Ring[i]->ring[j]->iph = (struct rte_ipv4_hdr*)calloc(1, sizeof(struct rte_ipv4_hdr));
//#endif
	RX_LCORE = 0; 
	PCAP_LCORE = RX_LCORE + conf->rx_que; 
	MONITOR_LCORE = PCAP_LCORE + conf->pr_que; 
	TIMER_LCORE = MONITOR_LCORE + 1; 
	CDF_LCORE = TIMER_LCORE + 1;
	TSTAMP_LCORE = CDF_LCORE + 1; 
	SSD_LCORE = TSTAMP_LCORE + 1; 
	
	SSD_Ring = (ssd_t*)calloc(conf->rx_que, sizeof(ssd_t));


#ifdef PMD_MODE
    pcap_file_header pfh;
    FILE *fp_in = fopen(PCAP_IN_FILE, "r");
    fp_out = fopen(PCAP_OUT_FILE, "w");
    fread(&pfh, sizeof(pcap_file_header), 1, fp_in);
    ret = fwrite(&pfh, sizeof(pcap_file_header), 1, fp_out);
    //fclose(fp_out);
    PP_Ring = (pp_que_t*)calloc(1, sizeof(pp_que_t));
    for(i = 0; i < conf->rx_que; i++){
        PP_Ring[i] = (pp_que_t)calloc(1, sizeof(struct pcap_queue));
        PP_Ring[i]->deque = PP_Ring[i]->occupy = 0;
        PP_Ring[i]->ring = (pcap_t*)calloc(conf->buffer_len, sizeof(pcap_t));
        pcap_t tmp = (pcap_t)calloc(conf->buffer_len, sizeof(struct to_pcap));
	for(j = 0; j < conf->buffer_len; j++){
            PP_Ring[i]->ring[j] = tmp[j];
	}
    }
#endif	

	check_all_ports_link_status(nb_ports, l2fwd_enabled_port_mask);

	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(l2fwd_launch_one_lcore, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	return 0;
}

