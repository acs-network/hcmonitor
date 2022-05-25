#ifndef _KEY_QUEUE_H
#define _KEY_QUEUE_H

#include <sys/time.h>

#include "config.h"

#define BUFSIZE 80000000
#define REBUFF  200000
#define MAX_QUE_NUM 6
#define PR_NUM 6
//#define QUESIZE (BUFSIZE / MAX_QUE_NUM)
uint32_t QUESIZE;

struct burst_tuple {
        uint32_t ip_src;
};

struct ipv4_2tuple {
	uint32_t ip_src;
	uint16_t port_src;	
};

struct http_tuple {
    int  status;
    uint16_t port_src;
    uint32_t ip_src;
};

struct node_data{
#if USE_HTTP
    int status;
    struct http_tuple key;
#elif MCC_DBG
    struct burst_tuple key;
#else
    struct ipv4_2tuple key;
#endif
    struct timespec ts;
    uint32_t sent_seq;
    uint32_t ack_seq;
    uint16_t total_len;
    char type;
    char pri;
};

typedef struct node_data *node_t;

struct pr_queue{
    node_t RxQue;
    volatile unsigned long occupy;
    volatile unsigned long deque;
};

typedef struct pr_queue *pr_que_t;

pr_que_t *PrQue;

typedef struct key_node
{
    struct node_data data;
    struct key_node* next;
}*KeyNode;

typedef struct Queue
{
  node_t* base; 
  unsigned long front,rear;	
  int occupy;
}LinkQueue;

LinkQueue *KeyQue;//(struct Queue*)malloc(sizeof(struct Queue));
LinkQueue *QueBak;
LinkQueue *Qcur;
LinkQueue *Qnext;
KeyNode Qbuf;


int InitQueue(void);
int EnQueue(LinkQueue *Q,struct node_data e);
int GetHead(LinkQueue *Q,struct node_data *e);
int DeQueue(LinkQueue *Q);
int QueueEmpty(LinkQueue *Q);
int FullQueue(LinkQueue *Q);
long QueueLength(LinkQueue *Q);


int send_key(LinkQueue *Q,struct node_data *data);

#endif

