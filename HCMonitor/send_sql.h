#ifndef _SENDSQL_H
#define _SENDSQL_H

#include <mysql/mysql.h>

#define CMD_BUF_SIZE 256
#define SQLPORT 3306

MYSQL g_conn;
MYSQL g_conn_10;
MYSQL g_conn_init; 
int insert_tag;
int timecnt;
unsigned int conn_peak;

int init_mysql(void) ;
void uninitializeMysql(void) ;
int executesql(const char * sql) ;
void print_mysql_error(const char *msg);
int insert_mysql_hy(float x,float y,int i);
int insert_mysql_init(int i);

int insert_mysql_inter(float delay,unsigned int conn);

#define DEBUG

uint32_t traffic;
uint32_t traffic_mid;


#endif

