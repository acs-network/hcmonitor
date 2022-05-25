/*************************************************************************
        > File Name: res_req.h
        > Author: 
        > Mail: 
        > Created Time: Jan 21 ,2016 16:05:25 PM PDT
 ************************************************************************/

#ifndef _RESREQ_H
#define _RESREQ_H

#include <sys/time.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include "rx_queue.h"
#include "send_sql.h"

//#define AREA_PRINT
//#define MTSC
//#define DEBUG
//#define ACKTEST 1
//#define MIRROR 1
//#define SAMPLE

//#define RETR_DEBUG
//#define PRI_DEBUG

//#define OP_MYSQL
//#define INTERVAL 60
#define TLT      26
#define SASP     260
int interval_cnt[SASP];
extern volatile unsigned long occupy;
extern volatile unsigned long deque;

//#define PMD_MODE

#ifdef PMD_MODE

FILE *fp_out;
#define PCAP_OUT_FILE "monitor.pcap"
#define PCAP_IN_FILE "input.pcap"
typedef struct pcap_file_header 
{
	uint32_t magic;
	uint16_t version_major;
	uint16_t version_minor;
	uint32_t thiszone;    
	uint32_t sigfigs;   
	uint32_t snaplen;   
	uint32_t linktype;  
} pcap_file_header;

typedef struct pcap_timestamp
{
	uint32_t ts_sec;
	uint32_t ts_usec;
} pcap_timestamp;

typedef struct pcap_header
{
	pcap_timestamp ts;
	uint32_t capture_len;
	uint32_t len;
} pcap_header;

struct to_pcap
{
	char *buff;
    pcap_header ph;
};

typedef struct to_pcap* pcap_t;

struct pcap_queue{
	pcap_t *ring;
	volatile unsigned long occupy;
    volatile unsigned long deque;
};

typedef struct pcap_queue* pp_que_t;

pp_que_t *PP_Ring;

#endif

//#define SSD_NUM 80000000


#if 0


uint64_t burst_pkts;
int *sample_rate;
unsigned int msec_count;
int bool_trans;
int bool_transed;
int bool_input;
int la;





//plot the cdf of time by python lib,just for no mysql 
void plotPoint2d(float *x, float *y);
void initializePlotting(void);
void uninitializePlotting(void);



unsigned long get_sample(uint64_t cur_tsc,uint64_t prev_tsc,uint64_t *timer_tsc);
#endif

/* dequeue the meta data and caculate response time */
unsigned int conn_active;
unsigned int conn;
unsigned int conn_active_mid;
unsigned int conn_active_pek;
float avg_delay;
float avg_delay_high;
float avg_delay_low;
float delay_99;//delay value @99%
unsigned int goodput; 
unsigned int conn;
unsigned int count;
unsigned int req_recount;
unsigned int rsp_recount;

struct atime{
	float *time;
	int *pri;
};

void clear(void);
float avg(float a[],unsigned int n);
int compInc(const void *a, const void *b);

void lcore_online(void);
void cdf_acktime(struct atime *ack_time_pro,unsigned int idx);


/* dequeue the meta data and caculate response time */
#define REQ_HASH_ENTRIES 1024*1024*64
#define BURST_HASH_ENTRIES 128
#define REQ_HASH_BUCKET  16

#define IP_NUM 6

#define HM 6

#define NM 1

volatile unsigned idx_pri_high;
volatile unsigned idx_pri_low;
volatile unsigned recv_pri_high;
volatile unsigned recv_pri_low;
volatile unsigned int conn_active_hy;
uint32_t pkt_num; //toatl pkts without ack
uint32_t pkt_req; //request pkts num 
uint32_t pkt_rep; //response pkts num
uint32_t prev_req; //request pkts num 
uint32_t prev_rep; //response pkts num
uint32_t miss_rep;//missed response 
uint32_t err;
unsigned int timeout_50;//pkts of response time over 50ms
unsigned int timeout_100;//pkts of response time over 100ms

int res_setup_hash(uint16_t socket_id);
int response_time_process(struct node_data *data,uint16_t nb_rx,uint16_t socket_id);


/* parse rhe packet and enqueue the meta data */
struct timespec ts;

int packet_process(struct rte_ipv4_hdr *ip_hdr, struct rte_tcp_hdr *tcp, struct timespec ts_now, int lcore_id, uint16_t payload_len);
int key_extract(struct rte_ipv4_hdr *ip_hdr,struct node_data *data,struct timespec ts_now,uint16_t payload_len);

int trans_pcap(struct rte_mbuf *m, struct timespec ts_now, int lcore_id);

volatile uint32_t burst[IP_NUM];
volatile char **ip_src;
volatile uint32_t request_num;
volatile uint32_t response_num;

#endif

