/*************************************************************************
	> File Name: config.h
	> Author: 
	> Mail: 
	> Created Time: Tue 19 May 2015 11:03:23 PM PDT
 ************************************************************************/

#ifndef _CONF_H
#define _CONF_H

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <libconfig.h>

#define MIRROR


#define MAXLABEL 2

enum packet_type
{
    M_REQ = 1,
    M_RSP = 2,
    M_UKO = 3
};

struct mconfig
{
    int  enable_http;    //network type IOT/normal HTTP. 1: HTTP, else IOT
    int  enable_https;   //test https traffic
    int  enable_hy;      //IOT traffic server platform. 1:Hy,else x86
	int  enable_mcc;

    int  server_port;    //server node port, used for judge https packet direction
    int  pkt_len;        //payload length, used for judge https packet priority
    
	/*packets receive cores and process cores*/
	int rx_que;          //packets receive queues(cores) num
	int pr_que;          //packets process queues(cores) num
	int interval;        //cdf calculate period(s)
	int buffer_len;      //buffered packets num in pr_queues

    /*parse request/response packet in IOT network*/
    int req_label[MAXLABEL];  //labels for request judge
    int resp_label[MAXLABEL]; //labels for response judge
    int label_offset;         //offset of label located in payload 
    int label_count;

    /*indicate request priority in IOT network*/
    int enable_pri;
	float pri_high_rate;
    int pri_offset;
    int pri_high_label;
    int pri_low_label;

    int enable_python;  //enable python print cdf
    int enable_sql;     //enable sql connected
};

struct mconfig* initConfig();
int getConfig(struct mconfig *conf);

struct mconfig *conf;

int enable_http;
int enable_mcc;

#define USE_HTTP 0

#define MCC_DBG  (enable_mcc)

uint32_t pkt_num;

char pf[10];

#endif

