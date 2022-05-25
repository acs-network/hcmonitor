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

#include <python2.7/Python.h>
#include <pthread.h>
#include "config.h"
#include "monitor.h"
#include "http_parse.h"

/* For packet metadata enqueue */
#define CAT_BUFF 80000000
/*#define PH_BUFF 10000000
#define PL_BUFF 10000000*/
unsigned long thre = 0;
unsigned long max_size;
unsigned long ph_size;
unsigned long pl_size;

/* For packet metadata hash and delay buffered*/
#define DEFAULT_HASH_FUNC  rte_jhash

/* Params for initial hash table*/
struct rte_hash_parameters ipv4_req_hash_params = {
       .name = NULL,
       .entries = REQ_HASH_ENTRIES,
       //.bucket_entries = REQ_HASH_BUCKET,
       //.key_len = sizeof(struct http_tuple),
       .hash_func = DEFAULT_HASH_FUNC,
       .hash_func_init_val = 0,
};

/* Params for initial hash table*/
struct rte_hash_parameters burst_hash_params = {
       .name = NULL,
       .entries = BURST_HASH_ENTRIES,
       //.bucket_entries = REQ_HASH_BUCKET,
       //.key_len = sizeof(struct http_tuple),
       .hash_func = DEFAULT_HASH_FUNC,
       .hash_func_init_val = 0,
};

struct req_vars{
	int count;
	int idx;
	int pri;
};

struct req_temp{
	uint32_t req_sent_seq;
	uint32_t ack_seq;
	uint32_t rsp_sent_seq;
	uint32_t ip_src;
	uint32_t port_src;
	uint16_t payload_len;
};

typedef struct rte_hash req_lookup_struct_t;

static req_lookup_struct_t *req_lookup_struct[2];
static req_lookup_struct_t *burst_lookup_struct[2];
static struct timespec *req_out_if;
static struct req_temp *Maxseq;

struct atime *ack_time_hy;
static struct req_vars *req_hy_count;

/* Save the ack_time online */
struct atime *ack_time;
float *ack_total;
float *ack_pri_high;
float *ack_pri_low;
unsigned long idx = 0;
unsigned long idx_hy = 0;
unsigned long idx_x86 = 0;

/* For print cdf online */
#define BLOCK 1000

float u[] = {0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,0.95,0.99,0.999,0.9999,1.0};

#ifdef AREA_PRINT
#define LLIMIT 0
#define ULIMIT 1
char arlog[20] = "latency_area.txt";
char artlog[20] = "det_latency.txt";
#endif

#ifdef RETR_DEBUG
char retra[20] = "retrans.txt";
#endif

int  min_count = 0;

int lock_flag;

//#define CMD_BUF_SIZE 512
char wzlog[30] = "cdf.txt";
char pri_low[20] = "cdf_low.txt";
char pri_high[20] = "cdf_high.txt";
#if 0
void initializePlotting(void) 
{
    Py_Initialize();
    // load matplotlib for plotting
    PyRun_SimpleString("from matplotlib import pyplot as pp");
    PyRun_SimpleString("pp.ion()"); // use pp.draw() instead of pp.show()
}

void uninitializePlotting(void) 
{
	Py_Finalize();
}

void plotPoint2d(float *x, float *y) 
{
		// this buffer will be used later to handle the commands to python
	static char command[CMD_BUF_SIZE];
	static char element_x[CMD_BUF_SIZE];
	static char element_y[CMD_BUF_SIZE];
	int i;
	
	/*
	 * convert the x,y data to string format so that python can execute command
	 * */
	snprintf(element_x, CMD_BUF_SIZE, "%f,", x[0]);
	snprintf(element_y, CMD_BUF_SIZE, "%f,", y[0]);
	for(i  = 1;i < POINT + 1;i++){
			sprintf(element_x, "%s%f,",element_x,x[i]);
			sprintf(element_y, "%s%f,",element_y,y[i]);			
	}
	sprintf(element_x,"%s%f", element_x,x[POINT+1]);
	sprintf(element_y,"%s%f", element_y,y[POINT+1]);
	/*end convert*/
    
	//printf("x:%s,y:%s\n",element_x,element_y);
	snprintf(command, CMD_BUF_SIZE, "pp.plot([%s],[%s],'r-')\npp.draw()",element_x,element_y);
	if(plot_flag)
		PyRun_SimpleString("pp.clf()");//if ex figure exists,clear it
	plot_flag = 1;
		PyRun_SimpleString(command);//update plot figure
}

unsigned long get_sample(uint64_t cur_tsc,uint64_t prev_tsc,uint64_t *timer_tsc)
{
    uint64_t diff_tsc;             
    diff_tsc = cur_tsc - prev_tsc;
	LinkQueue *Qlist[2] = {KeyQue,QueBak};

	/* if timer is enabled */
	if (timer_period > 0) {

		/* advance the timer */
		*timer_tsc += diff_tsc;

		/* if timer has reached its timeout */
		if (unlikely(*timer_tsc >= (uint64_t) timer_period)) {			             
            /* reset the timer */
            if(msec_count){
				Qcur = Qlist[msec_count % 2];
				bool_trans = 1;
			}
            *timer_tsc = 0;
            return 1;	

	    }else
	        return 0;
	}else
		return 0;
}
#endif

int compInc(const void *a, const void *b)  
{  
	const float *af = (const float *)a;
	const float *bf = (const float *)b;
	return *af > *bf ? 1 : -1;
} 	   

void clear(void)
{
	int i;
	for(i = 0; i < REQ_HASH_ENTRIES; i++){
        req_hy_count[i].count = 0;
        req_hy_count[i].idx = 0;
        req_hy_count[i].pri = -1;
    }
    memset(Maxseq, 0, sizeof(struct req_temp) * REQ_HASH_ENTRIES);
	memset(ack_time_hy->time, 0, sizeof(float) * CAT_BUFF);
    memset(ack_pri_high, 0, sizeof(float) * ph_size);
    memset(ack_pri_low, 0, sizeof(float) * pl_size);
    memset(interval_cnt, 0, sizeof(int) * SASP);

}

float avg(float a[],unsigned int n)
{
    unsigned int i;
    float result;
    float sum = 0;
#ifdef AREA_PRINT
	FILE *fp_ar = fopen(arlog,"a");
	unsigned int lower_limit = (int)(LLIMIT * n);
	unsigned int upper_limit = (int)(ULIMIT * n);
#endif
    for(i = 0;i < n;i++){
    	sum += a[i];
#ifdef AREA_PRINT
		if(i > lower_limit && i < upper_limit){
			fprintf(fp_ar,"%f\n",a[i]);
		}
#endif
	}

#ifdef AREA_PRINT
	fprintf(fp_ar,"===========\n");
	fclose(fp_ar);
#endif

    result = sum / n;
    return result;
}

float goodputcnt(float a[],unsigned int n)
{
	unsigned int good_cnt, i;
	float gpct;
    for(i = 0;i < n; i++){
		if(a[i] < 50.0){
			good_cnt++;
		}
	}
    gpct = good_cnt / n;
    return gpct;
}

void sketch_delay(float a[],unsigned int n)
{
	int i = 0, j;
	float step = (float)TLT / SASP;
    FILE *f_sk=fopen("sketch.txt","w");
    for( j = 1; j < SASP+1;){
    	for(;i < n; i++){
        	if(a[i] < step * j){
            	interval_cnt[j-1]++;        
        	}else{
                break;
			}
		}
        fprintf(f_sk, "%d\n", interval_cnt[j-1]);
		j++;
    }
    fclose(f_sk);
}

void cdf_acktime(struct atime *ack_time_pro,unsigned int idx)
{	
	float w[20];
	unsigned int index, i = 0;
	
	float w_high[20],w_low[20];
	unsigned int index_high,index_low,num_low = 0,num_high = 0;

	float gpct;

    if(conf->enable_pri){
    	if(idx_pri_high > ph_size){
        	ack_pri_high = (float*)realloc(ack_pri_high, sizeof(float) * (idx_pri_high + 1));
        	ph_size = idx_pri_high + 1;
    	}

    	if(idx_pri_low > pl_size){
        	ack_pri_low = (float*)realloc(ack_pri_low, sizeof(float) * (idx_pri_low + 1));
        	pl_size = idx_pri_low + 1;
    	}
	
		for(i = 0; i < idx; i++){
    		if(ack_time_pro->pri[i]){
        		ack_pri_high[num_high++] = ack_time_pro->time[i];
			}else{
            	ack_pri_low[num_low++] = ack_time_pro->time[i];
			}
    	}
		
		if(num_high){
			qsort(ack_pri_high,num_high,sizeof(float), compInc);
			avg_delay_high = avg(ack_pri_high,num_high);
		}
		if(num_low){
    		qsort(ack_pri_low,num_low,sizeof(float), compInc);
			avg_delay_low = avg(ack_pri_low,num_low);
		}

   	}
	avg_delay = avg(ack_time_pro->time, idx);
    gpct = goodputcnt(ack_time_pro->time, idx);
    qsort(ack_time_pro->time, idx, sizeof(float), compInc);
    sketch_delay(ack_time_pro->time, idx);

#ifdef OP_MYSQL
	//open the AUTOCOMMIT
	unsigned int t = 0;
 	t = mysql_real_query(&g_conn,"SET AUTOCOMMIT =0",(unsigned int)strlen("SET AUTOCOMMIT =0"));
 	if(t)
    {
     	printf("fail to open commit\n");
 	}
#endif

	FILE *fp = fopen(wzlog, "a");
	/*fprintf(fp, "connections:%d,retrans:%d,avg_delay:%f\n"
				"total : %4d, high : %4d, low : %4d\n",
			conn_active_mid,recount,avg_delay,idx,num_high,num_low);*/
	
	fprintf(fp, "connections:%d,req_retrans:%d,rsp_retrans:%d,avg_delay:%f\n",
			conn_active_mid,req_recount,rsp_recount,avg_delay);
    if(conf->enable_pri){
		fprintf(fp, "recv_pri_high:%d, recv_pri_low:%d.\n"
                "pri_high_num:%d,low_pri_num:%d\n",
            	idx_pri_high, idx_pri_low, num_high,num_low);
		fprintf(fp, "CDF: %9s: %18s: %26s:\n","total","high_pri","low_pri");
	}else{
		fprintf(fp, "CDF: %9s:\n","total");
	}
	req_recount = 0;
	rsp_recount = 0;

    goodput = (unsigned int)(conn * 0.05 * gpct) / conf->interval;
	
	if(conf->enable_pri){

		if(num_high && num_low){
    		fprintf(fp, "%d: %12.4f; %16.4f; %26.4f\n", 
				0, ack_time_pro->time[0], ack_pri_high[0], ack_pri_low[0]);
		}else if(num_high)
			fprintf(fp, "%d: %12.4f; %20.4f\n",
				0, ack_time_pro->time[0], ack_pri_high[0]);
	 	else if(num_low)
			fprintf(fp, "%d: %12.4f; %28.4f\n",
				0, ack_time_pro->time[0], ack_pri_low[0]);
	 	else
			fprintf(fp, "%d: %12.4f\n",
				0, ack_time_pro->time[0]);
	}

	printf("\n%d, %.4f\n", 0, ack_time_pro->time[0]);

	for(i = 0;u[i] != '\0';i++) //zhangwl@20160715
	{
		index = (int)ceil(u[i] * idx) - 1;
		w[i] = ack_time_pro->time[index];
		if(conf->enable_pri){
			if(num_high){
				index_high = (int)ceil(u[i] * num_high) - 1;
				w_high[i] = ack_pri_high[index_high];
			}
			if(num_low){
				index_low = (int)ceil(u[i] * num_low) - 1;
				w_low[i] = ack_pri_low[index_low];
			}
			if( u[i] > 0.999){
				printf("\n%.4f, %.4f\n", u[i], w[i]);
				if(num_high && num_low){
					fprintf(fp, "%.4f: %8.4f; %16.4f; %24.4f\n",
						u[i], w[i], w_high[i], w_low[i]);
				}else if(num_high){
					fprintf(fp, "%.4f: %8.4f; %16.4f\n", u[i],w[i],w_high[i]);
				}else if(num_low){
					fprintf(fp, "%.4f: %8.4f; %24.4f\n", u[i],w[i],w_low[i]);
				}else{
					fprintf(fp, "%.4f: %8.4f\n", u[i], w[i]);
				}
			}else{
				printf("\n%.2f, %.4f\n", u[i], w[i]);	
				if(num_high && num_low){
					fprintf(fp, "%.2f: %9.4f; %17.4f; %25.4f\n",
						u[i], w[i], w_high[i], w_low[i]);
				}else if(num_high){
					fprintf(fp, "%.2f: %9.4f; %17.4f\n", u[i],w[i],w_high[i]);
				}else if(num_low){
					fprintf(fp, "%.2f: %9.4f; %25.4f\n", u[i],w[i],w_low[i]);
				}else{
					fprintf(fp, "%.2f: %9.4f\n", u[i], w[i]);
				}
			}
		}else{
			if( u[i] > 0.999){
				fprintf(fp, "%.4f: %8.4f\n", u[i], w[i]);
			}else{
				fprintf(fp, "%.2f: %9.4f\n", u[i], w[i]);
			}
				
		}

#ifdef OP_MYSQL 
        // conn_active_mid =  conn_active_mid*2 ; /// INTERVAL * 60;
		if(insert_mysql_hy(w[i],u[i],i))
		    printf("Error when update row =%d\n",i);
		else
		    insert_tag = 1;
#endif
	}

	
	fclose(fp);
		
#ifdef OP_MYSQL              
    mysql_real_query(&g_conn,"COMMIT;",(unsigned int)strlen("COMMIT;"));
#endif

	/*if(idx){   //zhangwl@20160715
		printf("<%.2f:%.1f,<%.2f:%.1f,<%.2f:%.1f,<%.2f:%.1f,<%.2f:%.2f,<%.2f:%.2f,<%.2f:%.2f,<%.2f:%.2f\n",w[0],u[0],w[1],u[1],w[2],u[2],w[5],u[5],w[7],u[7],w[9],u[9],w[10],u[10],w[11],u[11]);
#ifdef PY_PLOT
		plotPoint2d(w,u);
#endif
	}else{
			printf("not receive data yet!\n");
	}*/
}

void lcore_online(void)
{
#ifdef OP_MYSQL
	int t = 0,i = 0;
    printf("start to insert init");
    for(i = 20; i > 0; i--)
	{
    	t = mysql_real_query(&g_conn_init,"SET AUTOCOMMIT =0",(unsigned int)strlen("SET AUTOCOMMIT =0"));
        if(t){
        	printf("fail to open commit\n");
        }else{
            if(insert_mysql_init(i))
            	printf("insert init error!\n");
            mysql_real_query(&g_conn_init,"COMMIT;",(unsigned int)strlen("COMMIT;"));
        }
	}
#endif

	if(lock_flag < BLOCK)
		lock_flag ++;
	
	while(1){
		//wait for several sec
		sleep(conf->interval);
	
		printf("hy_connections:%d\n",conn_active_hy);
		conn = conn_active_hy;
		conn_active_mid = conn_active_hy * 60 / conf->interval;
		conn_active_hy = 0;
		traffic_mid = traffic;
        	traffic = 0;

		min_count++;
	
		if(lock_flag && idx_hy){
			//printf("idx_hy:%ld\n",idx_hy);
			idx = idx_hy;
			memcpy(ack_time_hy->time,ack_time->time,sizeof(float) * idx_hy);
			memcpy(ack_time_hy->pri,ack_time->pri,sizeof(float) * idx_hy);
			idx_hy = 0;
			cdf_acktime(ack_time_hy,idx);
			clear();
    		idx_pri_low = 0;
    		idx_pri_high = 0;
    		recv_pri_low = 0;
    		recv_pri_high = 0;
		}else{
			lock_flag = 1;			
		}
		
	}
}

int response_time_process(struct node_data *data,uint16_t nb_rx,uint16_t socket_id)
{
    long ret = 0;
    long ret_req = 0;

    if (data->type == M_REQ)//REQU_USR || data->type == REQU_APP)//enter the request process:
    {
        pkt_req++; //count the request pkts
        
        ret_req = rte_hash_add_key(req_lookup_struct[socket_id],(void *) &(data->key));

        if (ret < 0) {
            printf("ret error!\n");
    		rte_exit(EXIT_FAILURE, "Unable to add entry %u\n",nb_rx);
    	}	
    	
		if(Maxseq[ret_req].req_sent_seq < data->sent_seq)
    	{
    		if(req_hy_count[ret_req].count == 0)
        	{
            	req_hy_count[ret_req].count = 1;
            	conn_active_hy++;
        	}else{
#ifdef RETR_DEBUG
			FILE *fp_flow = fopen(retra,"a");
			fprintf(fp_flow,"%d minutes flow ip_src:%lu.%lu,port_src:%lu,sent_seq:%lu,ack:%lu\n"
						  "====================\n",
							min_count,
							((data->key.ip_src >> 8) & 0xff),
							(data->key.ip_src & 0xff),
							data->key.port_src,
							data->sent_seq,
							data->ack_seq);
			fclose(fp_flow);
#endif
			}
    	    Maxseq[ret_req].req_sent_seq = data->sent_seq;
	    	Maxseq[ret_req].ip_src = data->key.ip_src;
            Maxseq[ret_req].port_src = data->key.port_src;
	    	Maxseq[ret_req].ack_seq = data->ack_seq;
	    	Maxseq[ret_req].payload_len = data->total_len;

    	    req_out_if[ret_req].tv_sec = data->ts.tv_sec;
    	    req_out_if[ret_req].tv_nsec = data->ts.tv_nsec;
        }else{
			req_recount++;
#ifdef RETR_DEBUG
			FILE *fp_flow = fopen(retra,"a");
			fprintf(fp_flow,"%d minutes req_pkt retrans ip_src:%lu.%lu,port_src:%lu,sent_seq:%lu,ack:%lu\n"
						  "====================\n",
							min_count,
							((data->key.ip_src >> 8) & 0xff),
							(data->key.ip_src & 0xff),
							data->key.port_src,
							data->sent_seq,
							data->ack_seq);
			fclose(fp_flow);
#endif
		}
		
        //traffic += data->total_len + 14; 
        if(data->pri == conf->pri_high_label){
	    	recv_pri_high++;				                 
	    	req_hy_count[ret_req].pri = 1;
        }else{
            recv_pri_low++;
	    	req_hy_count[ret_req].pri = 0;
		}


    	return 1;

    }else if(data->type == M_RSP)//RESP_APP || data->type == RESP_USR)
    {
        float ack = 0.0;
        struct timespec t_req;		
    	/*FILE *fp;
    	fp = fopen("response.txt","a+");*/
	
		pkt_rep++; //count the response pkts
        ret = rte_hash_lookup(req_lookup_struct[socket_id],(const void *)&(data->key));


		if ((data->sent_seq - Maxseq[ret].req_sent_seq > Maxseq[ret].payload_len)) {
#ifdef PRI_DEBUG
			FILE *fp_re = fopen(retra,"a");
			fprintf(fp_re,"response_pkt:ip_dst:%lu,port_dst:%lu,ack:%lu,sent_seq:%lu\n"
						  "request_pkt:ip_src:%lu,port_src:%lu,sent_seq:%lu,ack:%lu\n"
						  "seq_diff:%d\n"
			  			  "total_len:%d\n"
						  "====================\n",
							(data->key.ip_src & 0xff),
							data->key.port_src,
							data->sent_seq,
							data->ack_seq,
							(Maxseq[ret].ip_src & 0xff),
							Maxseq[ret].port_src,
							Maxseq[ret].req_sent_seq,
							Maxseq[ret].ack_seq,
							data->sent_seq - Maxseq[ret].sent_seq,
							data->total_len);
							fclose(fp_re);
#endif
			
			return -2;
        } 

		if (ret < 0){
			return -2;
		}

 	    t_req.tv_sec = req_out_if[ret].tv_sec;//-2 presents not find req
        t_req.tv_nsec = req_out_if[ret].tv_nsec;
		        	
		//if(!(nb_rx % 1)) 
		{
			ack = (data->ts.tv_sec - t_req.tv_sec) * 1000 
					+ ((float)(data->ts.tv_nsec - t_req.tv_nsec)/1000000);
	
			if(ack > 0)
			{
				if(idx_hy > thre)
                {
                    thre = idx_hy * 2;
                    ack_time->time = (float*)realloc(ack_time->time, sizeof(float) * thre);
                    ack_time->pri = (int*)realloc(ack_time->pri, sizeof(int) * thre);
                    ack_time_hy->time = (float*)realloc(ack_time_hy->time, sizeof(float) * thre);
                    ack_time_hy->pri = (int*)realloc(ack_time_hy->pri, sizeof(int) * thre);
                }
						
#ifdef AREA_PRINT
			    FILE *fp_art = fopen(artlog,"a");
			    uint64_t req_stamp = t_req.tv_sec * 1000000 + t_req.tv_nsec / 1000;
			    uint64_t resp_stamp = data->ts.tv_sec * 1000000 + data->ts.tv_nsec / 1000;
			    fprintf(fp_art,"%d,%lu,%lu,%f\n",
				req_hy_count[ret].idx,req_stamp,resp_stamp,ack);			
			    fclose(fp_art);
#endif
				if (data->sent_seq > Maxseq[ret].rsp_sent_seq){
					Maxseq[ret].rsp_sent_seq = data->sent_seq;
			    	if(req_hy_count[ret].pri){
				    	idx_pri_high++;				                 
				    	ack_time->pri[idx_hy] = 1;
			    	}else{
		    			idx_pri_low++;
						/*FILE *fp_lp = fopen("lp.txt","a");
						fprintf(fp_lp, "Recv low pri pkt:\n"
           				"hash_ret:%d\n"
		   				"src_ip:%d\n"
		   				"src_port:%d\n"
		   				"ack_seq:%d\n"
		   				"pri:%d\n"
                        "seq_diff:%d\n"
		   				"total_len:%d\n",
           				ret,
		   				data->key.ip_src,
		   				data->key.port_src,
		   				data->ack_seq,
		   				req_hy_count[ret].pri,
                        data->sent_seq - Maxseq[ret].sent_seq,
		   				data->total_len
						);
        				fclose(fp_lp);*/
				   	 ack_time->pri[idx_hy] = 0;
			    	}
					ack_time->time[idx_hy++] = ack;

					if (ack > 50)
				    	timeout_50++;
					}else{
						rsp_recount++;
#ifdef RETR_DEBUG
						FILE *fp_flow = fopen(retra,"a");
						fprintf(fp_flow,"%d minutes rsp_pkt retrans" 
								"ip_src:%lu.%lu,port_src:%lu,"
								"sent_seq:%lu,ack:%lu," 
								"response time:%.2f.\n"
						  		"====================\n",
								min_count,
								((data->key.ip_dst >> 8) & 0xff),
								(data->key.ip_dst & 0xff),
								data->key.port_dst,
								data->sent_seq,
								data->ack_seq, 
								ack);
						fclose(fp_flow);
#endif
						return -2;
					}	
		}					
					
    }
        return 1;
    }else
	    return -1;      
}

int res_setup_hash(uint16_t socket_id)
{
    char s[64];
    int i;
    /* create ipv4 hash */
	max_size = QUESIZE;
    if(conf->enable_http){
        ipv4_req_hash_params.key_len = sizeof(struct http_tuple);
    }else{
        ipv4_req_hash_params.key_len = sizeof(struct ipv4_2tuple);
    }
    burst_hash_params.key_len = sizeof(struct burst_tuple);
    snprintf(s, sizeof(s), "ipv4_hash_%d", socket_id);
    ipv4_req_hash_params.name = s;
    ipv4_req_hash_params.socket_id = socket_id;
    req_lookup_struct[socket_id] = rte_hash_create(&ipv4_req_hash_params);
    if (req_lookup_struct[socket_id] == NULL)
        rte_exit(EXIT_FAILURE, "Unable to create the request hash on "
                            "socket %d\n", socket_id);
    else
     	printf("Success creat request hash on socket %d\n",socket_id);
    
    snprintf(s, sizeof(s), "burst_hash_%d", socket_id);
    burst_hash_params.name = s;
    burst_hash_params.socket_id = socket_id;
    burst_lookup_struct[socket_id] = rte_hash_create(&burst_hash_params);
    if (burst_lookup_struct[socket_id] == NULL)
        rte_exit(EXIT_FAILURE, "Unable to create the burst hash on "
                            "socket %d\n", socket_id);
    else
        printf("Success creat request hash on socket %d\n",socket_id);

    ip_src = (char**)calloc(HM * NM, sizeof(char*));
    for(i=0; i < HM * NM; i++){
    	ip_src[i] = (char*)calloc(10, sizeof(char));
    }
	/*creat delay time store buffer*/
    thre = CAT_BUFF;

	if(conf->pri_high_rate > 0){
		ph_size = thre * conf->pri_high_rate;
		pl_size = thre - ph_size;
	}else{
		ph_size = pl_size = thre;
	}

    req_out_if = (struct timespec*)calloc(REQ_HASH_ENTRIES, sizeof(struct timespec));
    Maxseq = (struct req_temp*)calloc(REQ_HASH_ENTRIES, sizeof(struct req_temp));
    req_hy_count = (struct req_vars*)calloc(REQ_HASH_ENTRIES, sizeof(struct req_vars));
	
    ack_time = (struct atime*)calloc(1, sizeof(struct atime));
    ack_time->time = (float*)calloc(CAT_BUFF, sizeof(float));
    ack_time->pri = (int*)calloc(CAT_BUFF, sizeof(int));
    ack_time_hy = (struct atime*)calloc(1, sizeof(struct atime));
    ack_time_hy->time = (float*)calloc(CAT_BUFF, sizeof(float));
    ack_time_hy->pri = (int*)calloc(CAT_BUFF, sizeof(int));
    ack_pri_high = (float*)calloc(ph_size, sizeof(float));
    ack_pri_low = (float*)calloc(pl_size, sizeof(float));
    clear();
	memset(req_out_if, 0, sizeof(struct timespec) * REQ_HASH_ENTRIES);
	/*memset(ack_pri_high, 0, sizeof(float) * ph_size);
    memset(ack_pri_low, 0, sizeof(float) * pl_size);
    memset(Maxseq, 0, sizeof(struct req_temp) * REQ_HASH_ENTRIES);*/
    
    return 1;
}


int burst_count(struct rte_ipv4_hdr *ip_hdr,struct node_data *data,struct timespec ts_now)
{
    struct rte_tcp_hdr  *tcp;

    int id;

    int poffset = conf->label_offset;

    tcp = (struct rte_tcp_hdr *)((unsigned char *) ip_hdr + sizeof(struct rte_ipv4_hdr));
    unsigned char *req_bit = (unsigned char *) tcp + (tcp->data_off >> 2);

    if(!data){
        printf("node_data has been NULL!");
        return 0;
    }

    if (likely(req_bit[poffset] == conf->req_label[0] || req_bit[poffset] == conf->req_label[1])) 
    { 
        request_num++;        

        data->key.ip_src = rte_be_to_cpu_32(ip_hdr->src_addr);
 
        id = rte_hash_add_key(burst_lookup_struct[0],(void *) &(data->key));

        id = id % IP_NUM;

        sprintf(ip_src[id], "%u.%u", (data->key.ip_src >> 8) & 0xff, (data->key.ip_src & 0xff));

        burst[id]++;	
    }
    return 1;
}


int key_extract(struct rte_ipv4_hdr *ip_hdr,struct node_data *data,struct timespec ts_now, uint16_t payload_len)
{
    struct rte_tcp_hdr  *tcp;

    int poffset = conf->label_offset;

    tcp = (struct rte_tcp_hdr *)((unsigned char *) ip_hdr + sizeof(struct rte_ipv4_hdr));
    unsigned char *req_bit = (unsigned char *) tcp + (tcp->data_off >> 2);

    if(!data)
        printf("node_data has been NULL!");

    if (likely(req_bit[poffset] == conf->req_label[0] || req_bit[poffset] == conf->req_label[1])) 
    { 
 	    request_num++;

        data->key.ip_src = rte_be_to_cpu_32(ip_hdr->src_addr);
                
        data->key.port_src = rte_be_to_cpu_16(tcp->src_port);

        data->type = M_REQ;//req_bit[POFFSET];

		data->pri = req_bit[conf->pri_offset];

        data->total_len = payload_len;//rte_be_to_cpu_16(ip_hdr->total_length);
        
        data->sent_seq = rte_be_to_cpu_32(tcp->sent_seq);

		data->ack_seq = rte_be_to_cpu_32(tcp->recv_ack);

        data->ts.tv_sec = ts_now.tv_sec;

        data->ts.tv_nsec = ts_now.tv_nsec;

        return 1;
    }else if(req_bit[poffset] == conf->resp_label[0] || req_bit[poffset] == conf->resp_label[1])
    {
		
        response_num++;
        
        data->key.ip_src = rte_be_to_cpu_32(ip_hdr->dst_addr);
            
        data->key.port_src = rte_be_to_cpu_16(tcp->dst_port);

        data->type = M_RSP;//req_bit[POFFSET];

        data->total_len = payload_len;//rte_be_to_cpu_16(ip_hdr->total_length);

        data->sent_seq = rte_be_to_cpu_32(tcp->recv_ack);

		data->ack_seq = rte_be_to_cpu_32(tcp->sent_seq);

        data->ts.tv_sec = ts_now.tv_sec;

        data->ts.tv_nsec = ts_now.tv_nsec;

        return 1;
    }else{
	    //fprintf(stderr, "[EXCP]Unknown pakcet type,label bit:%d\n",req_bit[poffset]);
        return 0;
    }
}

int packet_process(struct rte_ipv4_hdr *ip_hdr, struct rte_tcp_hdr *tcp, struct timespec ts_now, int lcore_id, uint16_t payload_len)
{
    int la_key;
	node_t n;
	int i;
	i = lcore_id;
	while(likely((PrQue[i]->occupy + 1) % max_size == PrQue[i]->deque))
	{
		i = (i + 1) % MAX_QUE_NUM;
	}
	/*if(likely((PrQue[lcore_id]->occupy + 1) % max_size != PrQue[lcore_id]->deque)){
        n = PrQue[lcore_id]->RxQue + PrQue[lcore_id]->occupy;
	}else{
		printf("EXCP:Packet Process Queue %d is full!\n", lcore_id);
    	max_size = REBUFF + max_size;
    	PrQue[lcore_id]->RxQue = (node_t)realloc(PrQue[lcore_id]->RxQue,
                                max_size * sizeof(struct node_data));
    	n = PrQue[lcore_id]->RxQue + PrQue[lcore_id]->occupy;
    }*/
	n = PrQue[i]->RxQue + PrQue[i]->occupy;
#if USE_HTTP
	if(conf->enable_https)
		la_key = https_parse(ip_hdr, tcp, n, ts_now, payload_len);
	else
		la_key = http_parse(ip_hdr, tcp, n, ts_now, payload_len);
		
#else
	if(conf->enable_mcc)
		la_key = burst_count(ip_hdr, n, ts_now);
	else
		la_key = key_extract(ip_hdr, n, ts_now, payload_len); 
#endif
	if(likely(la_key > 0))
    {       
		PrQue[lcore_id]->occupy = (PrQue[lcore_id]->occupy + 1) % max_size;
		_mm_sfence();
        return 1;			
    }else{
    	//printf("Enqueue failed\n");
        return la_key;		
    }		
		
}

