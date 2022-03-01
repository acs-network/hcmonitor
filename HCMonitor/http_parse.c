#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <sys/types.h>
#include <string.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/param.h>
#include <pthread.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_debug.h>
#include <rte_distributor.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_string_fns.h>
#include <rte_lpm.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_fbk_hash.h>
#include <rte_hash.h>
#include <rte_jhash.h>

#include <pthread.h>
#include "config.h"

#include "http_parse.h"

#if USE_HTTP
uint8_t http_parse(struct rte_ipv4_hdr *ip_hdr,struct rte_tcp_hdr *tcp,struct node_data *data,struct timespec ts_now, uint16_t payload_len)
{
    char ch,sh;
    data->status = -1;
    unsigned char *p = (unsigned char *) tcp + (tcp->data_off >> 2); 

    ch = *p;	

    switch(ch){
        case 'G':
            data->type = M_REQ;
	        data->status = GET;
	    break;
        case 'H':
	        sh = *(p + 1);
	        switch(sh){
	            case 'T':
	                sh = *(p + 9);
	                switch(sh){
	                    case '2':
		                if((*(p + 10) == '0') && (*(p + 11) == '0')){	
		                    data->status = GET;
		                    data->type = M_RSP;
		                }
		                break;
                        case '4':
                        if(*(p + 10) == '0'){    
                            data->status = GET;
                            data->type = M_RSP;
	                    }
	                    break;
                    }
                break;
                case 'E':
                    data->status = HEAD;
                    data->type = M_REQ;
                break;
           }
           break;
        case 'P':
            sh = *(p+1);
            data->type = M_REQ;
            switch(sh){
                case 'O':
                data->status = POST;
                break;
                case 'U':
                data->status = PUT;
                break;
            }
        break;
        case 'D':
            data->status = DELETE;
            data->type = M_REQ;
        break;
        case 'T':
            data->status = TRACE;
            data->type = M_REQ;
        break;
     }

    if (likely(data->type == M_REQ))
    {
	data->pri = p[conf->pri_offset];
 
        data->key.ip_src = rte_be_to_cpu_32(ip_hdr->src_addr);
                
        data->key.port_src = rte_be_to_cpu_16(tcp->src_port);

        data->key.status = data->status;

        data->total_len = payload_len;//rte_be_to_cpu_16(ip_hdr->total_length);

        data->sent_seq = rte_be_to_cpu_32(tcp->sent_seq);

        data->ts.tv_sec = ts_now.tv_sec;

        data->ts.tv_nsec = ts_now.tv_nsec;

        return 1;
    }else if(data->type == M_RSP)
    {
        
        data->key.ip_src = rte_be_to_cpu_32(ip_hdr->dst_addr);
            
        data->key.port_src = rte_be_to_cpu_16(tcp->dst_port);

        data->total_len = payload_len;//rte_be_to_cpu_16(ip_hdr->total_length);
        
        data->key.status = data->status;

        data->sent_seq = rte_be_to_cpu_32(tcp->recv_ack);

        data->ts.tv_sec = ts_now.tv_sec;

        data->ts.tv_nsec = ts_now.tv_nsec;

        return 1;
    }else{
//		printf("status:%d\n",data->status);
        return 0;
	}
	
}

uint8_t https_parse(struct rte_ipv4_hdr *ip_hdr, struct rte_tcp_hdr *tcp, struct node_data *data,struct timespec ts_now, uint16_t payload_len)
{
    uint16_t src_port,dst_port;

    dst_port = rte_be_to_cpu_16(tcp->dst_port);
    src_port = rte_be_to_cpu_16(tcp->src_port);

    if(!data){
        printf("node_data has been NULL!");
        return 0;
    }

    if (dst_port == conf->server_port)
    {

        data->key.ip_src = rte_be_to_cpu_32(ip_hdr->src_addr);

        data->key.port_src = src_port;

        data->type = M_REQ;//req_bit[POFFSET];

		if(payload_len > conf->pkt_len) {
			data->pri = conf->pri_high_label;
		} else {
			data->pri = 0;
		}

        data->total_len = payload_len;//rte_be_to_cpu_16(ip_hdr->total_length);

        data->sent_seq = rte_be_to_cpu_32(tcp->sent_seq);

        data->ack_seq = rte_be_to_cpu_32(tcp->recv_ack);

        data->ts.tv_sec = ts_now.tv_sec;

        data->ts.tv_nsec = ts_now.tv_nsec;

        return 1;
    }else if(src_port == conf->server_port)
    {
        data->key.ip_src = rte_be_to_cpu_32(ip_hdr->dst_addr);
            
        data->key.port_src = dst_port;
        
        data->type = M_RSP;

        data->total_len = payload_len;//rte_be_to_cpu_16(ip_hdr->total_length);
        
        data->key.status = data->status;

        data->sent_seq = rte_be_to_cpu_32(tcp->recv_ack);

        data->ts.tv_sec = ts_now.tv_sec;

        data->ts.tv_nsec = ts_now.tv_nsec;

        return 1;
        
    }else
        return 0;
}
#endif


