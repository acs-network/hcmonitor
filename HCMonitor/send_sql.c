#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include "config.h"
#include "send_sql.h"

extern unsigned int conn_active;
extern float avg_delay;
extern unsigned int goodput;
extern unsigned int conn_active_pek;
extern unsigned int conn_active_mid;
extern void clear(void);

int init_mysql(void) { 
     FILE *fp_sql;
     char g_host_name[24],g_user_name[24],g_password[24],g_db_name[24];
	 
     /*get the db config from txt*/
     fp_sql = fopen("config_db","r");
     
     fscanf(fp_sql,"%s%s%s%s",g_host_name,g_user_name,g_password,g_db_name);
     
     /*init the database connection*/
     mysql_init(&g_conn);

     /* connect the database */
     if(!mysql_real_connect(&g_conn, g_host_name, g_user_name, g_password, 
        g_db_name, SQLPORT, NULL, 0)) 
     {
         // if failed
         printf( "Error connecting to database: %s\n",mysql_error(&g_conn));
         return -1;
     }
    else 
         printf("Connected\n");
    
     if(!mysql_real_connect(&g_conn_10, g_host_name, g_user_name, g_password, 
        g_db_name, SQLPORT, NULL, 0))
     {
        printf( "Error connecting to database: %s\n",mysql_error(&g_conn));
        return -1;
     }
     else
        printf("g_conn_10 Connected\n");

    if(!mysql_real_connect(&g_conn_init, g_host_name, g_user_name, g_password, 
        g_db_name, SQLPORT, NULL, 0))
     {
        printf( "Error connecting to database: %s\n",mysql_error(&g_conn));
        return -1;
     }
     else
        printf("g_conn_init Connected\n");

     // set the server platform
     if(conf->enable_hy)
        strcpy(pf, "hy");
     else
        strcpy(pf, "x86");

     // test if connection can be used
     if (executesql("set names utf8")) // fail
         return -1;

    return 0; // return success value
}

void uninitializeMysql(void) 
{
	mysql_close(&g_conn); // close connection
	mysql_close(&g_conn_10);
    mysql_close(&g_conn_init);
}

int executesql(const char * sql) {
    /*query the database according the sql*/
    if (mysql_real_query(&g_conn, sql, strlen(sql)))
        return -1; // return for fail

    if (mysql_real_query(&g_conn_10, sql, strlen(sql)))
        return -1;

    if(mysql_real_query(&g_conn_init, sql, strlen(sql)))
        return -1;

    return 0; // return success value
}


void print_mysql_error(const char *msg) { 
    if (msg)
        printf("%s: %s\n", msg, mysql_error(&g_conn));
    else
        puts(mysql_error(&g_conn));
}

int insert_mysql_hy(float x,float y,int i)
{
    char sql[CMD_BUF_SIZE],sql_model[CMD_BUF_SIZE],sql_conn[CMD_BUF_SIZE],sql_99[CMD_BUF_SIZE];
	int t = 0,t_model = 0,t_conn =0,t_99 = 0;
	struct timeval tv;
	memset(sql,0,CMD_BUF_SIZE); 
	memset(sql_model,0,CMD_BUF_SIZE);
	memset(sql_conn,0,CMD_BUF_SIZE);
	memset(sql_99,0,CMD_BUF_SIZE);
	if(conn_active_pek < conn_active_mid)
		conn_active_pek = conn_active_mid;
        //if(insert_tag)
    sprintf(sql,"update model_chart_%s set idx='%.2f',delay='%.2f' where id=%d",pf, y,x,i+1);
	//sprintf(sql_model,"update model_chart_hy set concurrency='%d',delay='%.2f'where id=1",conn_active_hy,avg_delay_hy);
	if(i == 10)
	{
		//printf("conn_active_hy:%d,avg_delay_hy:%f\n",conn_active_mid,avg_delay);
		printf("conn_active_hy:%d,goodput_hy:%d\n",conn_active_mid,goodput);
		gettimeofday(&tv,NULL);
		/*sprintf(sql_model,"update model_%s set concurrency='%d',delay='%.2f'where id=1",
            pf, conn_active_pek,avg_delay);*/
		sprintf(sql_model,"update model_%s set concurrency='%d',delay='%d'where id=1",
            pf, conn_active_pek,goodput);
		sprintf(sql_conn,"insert into model_chart_concurrency_%s(idx,num) values('%ld','%d')",
            pf, tv.tv_sec, conn_active_mid);
        printf("traffic/s:%fMbps\n",(float)(traffic_mid)*8/(60*1000000));
		//conn_active_hy = 0;
		clear();
		sprintf(sql_99,"insert into model_chart_99_%s(idx,num) values('%ld','%.2f')",pf, tv.tv_sec, x);
		if(timecnt > 100)
			timecnt = 0;
	}
        //else
                //sprintf(sql,"insert into model_chart(id,index,delay) values('%d','%f','%f')",i+1,y,x);
        //printf("cmd out\n");
        t = mysql_real_query(&g_conn,sql,(unsigned int)strlen(sql));
	//t_model = mysql_real_query(&g_conn,sql_model,(unsigned int)strlen(sql_model));
	if(i == 10)
	{
		t_model = mysql_real_query(&g_conn,sql_model,(unsigned int)strlen(sql_model));
		fprintf(stderr, "Send conn to sql cmd:%s\n", sql_conn);
		t_conn = mysql_real_query(&g_conn,sql_conn,(unsigned int)strlen(sql_conn));
		t_99 = mysql_real_query(&g_conn,sql_99,(unsigned int)strlen(sql_99));
		if(t || t_model || t_conn || t_99){ 
			fprintf(stderr, "send to sql fail \n"
				   "model_chart_%s:%d\n" 
				   "model_%s:%d\n"
				   "model_chart_concurrency_%s:%d\n"
				   "model_chart_99_%s:%d\n",
					pf, t, 
					pf, t_model,
                    pf, t_conn,
					pf, t_99);
			return -1;
		}
     }
     if(t)
        return -1;
     else
        return 0;
}

int insert_mysql_init(int i)
{
	char sql_conn[CMD_BUF_SIZE],sql_99[CMD_BUF_SIZE],sql_model[CMD_BUF_SIZE];
	int t_conn =0,t_99 = 0,t_model = 0;
	struct timeval tv;
	
	memset(sql_conn,0,CMD_BUF_SIZE);
	memset(sql_99,0,CMD_BUF_SIZE);
    memset(sql_model,0,CMD_BUF_SIZE);
	
	//for(i = 20;i > 0;i--){
	gettimeofday(&tv,NULL);
	sprintf(sql_conn,"insert into model_chart_concurrency_%s(idx,num) values('%ld','%d')",
        pf, tv.tv_sec - 10 * i, 0);
    sprintf(sql_99,"insert into model_chart_99_%s(idx,num) values('%ld','%.2f')",
        pf, tv.tv_sec - 10 * i, 0.00);
	sprintf(sql_model,"update model_%s set concurrency='%d',delay='%.2f'where id=1", 
        pf, 0, 0.00);
	//}
	
	if(i > 0)
	{
		t_conn = mysql_real_query(&g_conn_init,sql_conn,(unsigned int)strlen(sql_conn));
		t_99 = mysql_real_query(&g_conn_init,sql_99,(unsigned int)strlen(sql_99));
		t_model = mysql_real_query(&g_conn_init,sql_model,(unsigned int)strlen(sql_model));
		if(t_conn || t_99 || t_model) 
			return -1;
        }
	
	return 0;
}

int insert_mysql_inter(float delay,unsigned int conn)
{
        char sql_conn[CMD_BUF_SIZE],sql_99[CMD_BUF_SIZE];
        int t_conn =0,t_99 = 0;
        struct timeval tv;

        memset(sql_conn,0,CMD_BUF_SIZE);
        memset(sql_99,0,CMD_BUF_SIZE);

        gettimeofday(&tv,NULL);
        sprintf(sql_conn,"insert into model_chart_concurrency_%s(idx,num) values('%ld','%d')",
            pf, tv.tv_sec, conn);
        sprintf(sql_99,"insert into model_chart_99_%s(idx,num) values('%ld','%.2f')",
            pf, tv.tv_sec, delay);

        t_conn = mysql_real_query(&g_conn_10,sql_conn,(unsigned int)strlen(sql_conn));
        t_99 = mysql_real_query(&g_conn_10,sql_99,(unsigned int)strlen(sql_99));
        if(t_conn || t_99)
                return -1;
        
	return 0;

}
