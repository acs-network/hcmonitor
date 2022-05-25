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

#include <python2.7/Python.h>
#include <pthread.h>
#include "rx_queue.h"

unsigned long max = BUFSIZE;

int InitQueue(void)
{
    int i;
	QUESIZE = BUFSIZE / conf->rx_que;
    PrQue = (pr_que_t*)calloc(conf->rx_que, sizeof(pr_que_t));
    for(i = 0; i < MAX_QUE_NUM; i++){
        PrQue[i] = (pr_que_t)calloc(1, sizeof(struct pr_queue));
        PrQue[i]->deque = 0;
        PrQue[i]->occupy = 0;
        PrQue[i]->RxQue = (node_t)malloc(QUESIZE * sizeof(struct node_data));
    }
    return 1;
}

#if 0
int EnQueue(LinkQueue *Q,struct node_data e)
{
	if(FullQueue(Q)){
		/*Queue full,add extra BUFSIZE space*/
		Q->base=(QType*)realloc(Q->base,(Q->rear + BUFSIZE)*sizeof(struct node_data));
		if(!Q->base)
			return 0;
		max = BUFSIZE * 2;
	}
		
	*(Q->base + Q->rear) = e;
	Q->rear = (Q->rear + 1) % max;
	//occupy++;
    return 1;
}

int GetHead(LinkQueue *Q,struct node_data *e)
{
  //if the queue is not empty,get the head node
  if(Q){
  	if(Q->front == Q->rear)
    	return 0;
	*e = *(Q->base+Q->front); 
  	if(e){
		occupy++;
		return 1;
  	}else{	
		//long length = QueueLength(Q);
		//printf("GetHead p:%p,Q->front:%p,Q->rear:%p\n",p,Q->front,Q->rear);
		rte_exit(EXIT_FAILURE, "GetHead p:%p,Q->front:%lu,Q->rear:%lu,occupy:%ld,deque:%ld\n",e,Q->front,Q->rear,occupy,deque);
		return 0;
  	}
  }else
	return 0;
}

int DeQueue(LinkQueue *Q)
{
  //if the queue is not empty,delete the head node
  if(Q){
	if(Q->front==Q->rear)
		return 0;
	Q->front = (Q->front + 1) % max;
	deque++;
	return 1;
  }else{
	return 0;
  }
}

int QueueEmpty(LinkQueue *Q)
{//if the queue is empty,return true

  if(Q->front == Q->rear)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

int FullQueue(LinkQueue *Q)  
{  
    if(Q->front == (Q->rear + 1) % max)  
        return 1;  
    else  
        return 0;  
}  

long QueueLength(LinkQueue *Q)
{
  //get the queue length
  if(Q)
  	return(Q->rear-Q->front);
  else
	rte_exit(EXIT_FAILURE,"QueueLength Get failed Due to NULL Q!\n");
}

int send_key(LinkQueue *Q,struct node_data *data)
{
    if(EnQueue(Q,*data))
        return 1;
    else
        return 0;
}

#endif
