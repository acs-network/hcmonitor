#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<netinet/in.h>
#include<time.h>
/*
程序目标：
1 计算一个聚合burst内每个小burst的包间隔。
2 输出每个小burst的源ip，总数， 包间隔10分位到99分位情况。
3 计算一个聚合burst的包间隔
4 输出聚合burst的总数，包间隔10分位到99分位情况。

程序基本流程：
1 pcap遍历处理
    根据每个数据包的网络号，将该数据包内所需数据记录并分类到burst组中(burst组数据结构：burst_pkts)
2 遍历每个burst组计算
    计算包间隔记录到该burst组的包间隔数组中。
3 遍历每个burst组,输出包间隔数据。


步骤1 pcap遍历处理流程
遍历每一个包
    1） 如果是新ip，创建新的ip组;
    2） 将时间戳放入对应ip的pkt组中;

2 步骤2 遍历每个burst组计算流程
    计算每个包之间包间隔并记录包间隔数组;
    包间隔数组快排形成有序包间隔数组;

3 遍历每个burst组,输出包间隔数据流程
    对每个burst组，输出基本信息

*/

/*
*宏定义
*/

#define RTE_ETHER_TYPE_VLAN 0x8100
#define RTE_ETHER_ADDR_LEN  6

#define PKT_NUM 1000000
#define BUFSIZE 10240
#define STRSIZE 1024

/*
* 别名定义
*/
typedef uint16_t rte_be16_t; /**< 16-bit big-endian value. */
typedef uint32_t rte_be32_t; /**< 32-bit big-endian value. */
typedef uint64_t rte_be64_t; /**< 64-bit big-endian value. */  
typedef uint16_t rte_le16_t; /**< 16-bit little-endian value. */
typedef uint32_t rte_le32_t; /**< 32-bit little-endian value. */
typedef uint64_t rte_le64_t; /**< 64-bit little-endian value. */

/*
*数据结构定义
*/

/*pacp文件头结构体*/
struct pcap_file_header
{
    uint32_t magic;       /* 0xa1b2c3d4 */
    uint16_t version_major;   /* magjor Version 2 */
    uint16_t version_minor;   /* magjor Version 4 */
    int32_t thiszone;      /* gmt to local correction */
    uint32_t sigfigs;     /* accuracy of timestamps */
    uint32_t snaplen;     /* max length saved portion of each pkt */
    uint32_t linktype;    /* data link type (LINKTYPE_*) */
};

/*时间戳*/
struct time_val
{
    int tv_sec;         /* seconds 含义同 time_t 对象的值 */
    int tv_usec;        /* microseconds */
};

/*pcap数据包头结构体*/
struct pcap_pkthdr
{
    struct time_val ts;  /* time stamp */
    uint32_t caplen; /* length of portion present */
    uint32_t len;    /* length this packet (off wire) */
};

/*mac地址结构体*/
struct rte_ether_addr {
	uint8_t addr_bytes[RTE_ETHER_ADDR_LEN]; /**< Addr bytes in tx order */
} __attribute__((aligned(2)));

/*数据帧头*/
struct rte_ether_hdr {
	struct rte_ether_addr d_addr; /**< Destination address. */
	struct rte_ether_addr s_addr; /**< Source address. */
	uint16_t ether_type;      /**< Frame type. */
} __attribute__((aligned(2)));

/*IP数据报头*/
struct rte_ipv4_hdr {
	uint8_t  version_ihl;		/**< version and header length */
	uint8_t  type_of_service;	/**< type of service */
	rte_be16_t total_length;	/**< length of packet */
	rte_be16_t packet_id;		/**< packet ID */
	rte_be16_t fragment_offset;	/**< fragmentation offset */
	uint8_t  time_to_live;		/**< time to live */
	uint8_t  next_proto_id;		/**< protocol ID */
	rte_be16_t hdr_checksum;	/**< header checksum */
	rte_be32_t src_addr;		/**< source address */
	rte_be32_t dst_addr;		/**< destination address */
} __attribute__((__packed__));

/*TCP数据报头*/
struct rte_tcp_hdr {
	rte_be16_t src_port; /**< TCP source port. */
	rte_be16_t dst_port; /**< TCP destination port. */
	rte_be32_t sent_seq; /**< TX data sequence number. */
	rte_be32_t recv_ack; /**< RX data acknowledgment sequence number. */
	uint8_t  data_off;   /**< Data offset. */
	uint8_t  tcp_flags;  /**< TCP flags */
	rte_be16_t rx_win;   /**< RX flow control window. */
	rte_be16_t cksum;    /**< TCP checksum. */
	rte_be16_t tcp_urp;  /**< TCP urgent pointer, if any. */
} __attribute__((__packed__));

/*
*全局变量定义
*/
int pkt_gap[PKT_NUM];
double variance=0;
double avg=0;

/**/
struct burst_pkts {
    rte_be32_t src_ip;     //对应ip
    int len;               //该ip的数据包数量
    struct time_val *ts;   //该ip的数据包时间戳数组
    int *gaps;             //该ip的包间隔数组
	int width;
};

/*存放五个client的ip*/
struct ip_groups{
    int len;
    rte_be32_t src_ip[100];
};

struct burst_pkts bursts_global[10];
struct ip_groups ip_groups_global;


void mynstrcpy(char *target, char *source, int n)
{
	int i = 0;
	while((i < n) && (*target = *source) != '\0')
	{
		source++;
		target++;
		i++;
	}
}

/*该ip已存在则返回对应序号，返回-1表示不存在*/
int exist(rte_be32_t addr){
    int i=0;
    for(i=0;i<ip_groups_global.len;i++){
        //printf("new ip is %d.%d.%d.%d\n",addr >> 24 & 0xff,addr>>16 & 0xff,addr>>8 & 0xff,addr& 0xff);
        //printf("old ip is %d.%d.%d.%d\n",ip_groups_global.src_ip[i] >> 24 & 0xff,ip_groups_global.src_ip[i]>>16 & 0xff,ip_groups_global.src_ip[i]>>8 & 0xff,ip_groups_global.src_ip[i]& 0xff);
        //printf("new ip >>8 is %d\n",addr >> 8);
        //printf("old ip >>8 is %d\n",ip_groups_global.src_ip[i] >> 8);
        if((ip_groups_global.src_ip[i] >> 8 )==(addr >> 8 ))
            return i;
    }
    return -1;
}
/*添加对应的ip*/
int create(rte_be32_t addr){
    bursts_global[ip_groups_global.len].src_ip=(addr>>8)<<8;
    printf("creat success,new ip is %d.%d.%d.%d ,group is %d\n",addr >> 24 & 0xff,addr>>16 & 0xff,addr>>8 & 0xff,addr& 0xff,ip_groups_global.len);
    ip_groups_global.src_ip[ip_groups_global.len]=addr;
    ip_groups_global.len++;
    if(ip_groups_global.len>=10){
        fprintf(stderr,"create fail!!!!");
        exit(-1);
    }
    return ip_groups_global.len-1;
}

/*对应ip组中添加新的数据包*/
void add(rte_be32_t src_addr,struct time_val ts ,struct time_val last_ts){
    int host=exist(src_addr);
    if(host==-1){
        host=create(src_addr);
    }
    bursts_global[host].len++;
    bursts_global[host].ts[bursts_global[host].len]=ts;
    //printf("add finish,bursts_global[host].ts[bursts_global[host].len].usec is %d\n",bursts_global[host].ts[bursts_global[host].len].tv_usec);
}

/*快排组件*/
int PartSort(int*arr,int first,int end)     //分步排序函数
{
	int tmp=arr[first]; //取第一个数作为基准值
	while(first!=end)
	{                                      //取左边作为基准值从右面开始判断
		while(first<end && arr[end]>=tmp)  //判断end指向的值是否大于基准值
			end--;                         //如果大于，end向左移动
		arr[first]=arr[end];               //否则将值放到左边
		while(first<end && arr[first]<=tmp)//判断first指向的值是否小于基准值
			first++;                       //如果小于，first向右移动
		arr[end]=arr[first];               //否则将值放到右边
	}
	arr[first]=tmp;                        //指针相遇，将基准值放在该位置
	return first;                          //返回相遇位置
}

/*快排*/
void Quick(int*arr,int first,int end)//定义一个头指针和尾指针
{
	if(first<end)
	{
		int mid = PartSort(arr,first,end);
		Quick(arr,first,mid-1);            //已经排好序的基准值不参与下次排序
		Quick(arr,mid+1,end);
	}
}

/*
* 函数定义
* pcap遍历处理
*     根据每个数据包的网络号，将该数据包内所需数据(记录并分类到burst组中(burst组数据结构：burst_pkts)
* 遍历每一个包
*     1） 如果是新ip，创建新的ip组;
*     2） 将时间戳放入对应ip的pkt组中;
*/
void process_pcap(char* file){
    struct pcap_file_header *file_header;
    struct pcap_pkthdr *pkt_header;
	struct pcap_pkthdr *pkt_header_0;
	struct pcap_pkthdr *pkt_header_last;
    struct rte_ether_hdr *eth_header;
    struct rte_ipv4_hdr *ip_header;
    FILE *fp, *output;
    int  pkt_offset, i = 0;
    int  http_len, ip_proto;
 
    char buf[BUFSIZE], my_time[STRSIZE];
    char src_ip[16], tmp_ip_net[8];
    char  host[STRSIZE], uri[BUFSIZE];
    rte_be32_t tmp_src_ip;
    int tmp=1;
    //初始化
    pkt_header = (struct pcap_pkthdr *)malloc(sizeof(struct pcap_pkthdr));
    //pkt_header_0 = (struc t pcap_pkthdr *)malloc(sizeof(struct pcap_pkthdr));
	eth_header= (struct rte_ether_hdr *)malloc(sizeof(struct rte_ether_hdr));
    ip_header=(struct rte_ipv4_hdr *)malloc(sizeof(struct rte_ipv4_hdr));
    if ((fp = fopen(file, "r")) == NULL)
    {
        printf("error: can not open pcap file\n");
        exit(0);
    }
    //开始读数据包
    pkt_offset = 24; //pcap文件头结构 24个字节
    while (fseek(fp, pkt_offset, SEEK_SET) == 0) //遍历数据包
    {
        //pcap_pkt_header 16 byte
        //pkt_header_last=pkt_header;
		memset(pkt_header, 0, sizeof(struct pcap_pkthdr));
		
        if (fread(pkt_header, 16, 1, fp) != 1) //读pcap数据包头结构
        {
            printf("\nread end of pcap file\n");
            break;
        }
        pkt_offset += 16 + pkt_header->caplen;   //下一个数据包的偏移值
     	if(tmp==1){
			//pkt_header
		}
				   
		//数据帧头 14字节
        memset(eth_header, 0, sizeof(struct rte_ether_hdr));
        if (fread(eth_header, sizeof(struct rte_ether_hdr), 1, fp) != 1)
        {
            printf("%d: can not read ip_header\n", i);
            continue;
        }
        //忽略vlan头
        if(ntohs(eth_header->ether_type)==RTE_ETHER_TYPE_VLAN){
            fseek(fp, 4, SEEK_CUR);
        }
        //IP数据报头 20字节
        memset(ip_header, 0, sizeof(struct rte_ipv4_hdr));
        if (fread(ip_header, sizeof(struct rte_ipv4_hdr), 1, fp) != 1)
        {
            printf("%d: can not read ip_header\n", i);
            continue;
        }
        add(ntohl(ip_header->src_addr),pkt_header->ts,pkt_header_last->ts);
        i++;
    } // end while
    fclose(fp);
    free(pkt_header);
}
/*
2 遍历每个burst组计算
    计算每个包之间包间隔并记录包间隔数组;
    包间隔数组快排形成有序包间隔数组;
*/
void compute_data(){
    int i,j;
    for(i=0;i<ip_groups_global.len;i++){
        printf("burst group %d , compute start\n",i);
        for(j=0;j<bursts_global[i].len-1;j++){
            bursts_global[i].gaps[j]=bursts_global[i].ts[j+1].tv_usec-bursts_global[i].ts[j].tv_usec;
            //printf("first usec is %d,newxt usec is %d\n",(bursts_global[i+1].ts).tv_usec,(bursts_global[i].ts).tv_usec);
            //printf("compute_data ,bursts_global[i].gaps[i] is %d\n",bursts_global[i].gaps[j]);
        }
        Quick(&bursts_global[i].gaps[i],0,bursts_global[i].len-1);
        int sec=bursts_global[i].ts[bursts_global[i].len-1].tv_sec-bursts_global[i].ts[1].tv_sec;
        int usec=bursts_global[i].ts[bursts_global[i].len-1].tv_usec-bursts_global[i].ts[1].tv_usec;
        if(sec==1&&usec<=0){
            bursts_global[i].width=1000+usec/1000;
        }
        else if (sec==0&&usec>=0){
            bursts_global[i].width=usec/1000;
        }
        else{
            printf("unexpected compute result,exit!!\n");
            printf("first sec is %d,first usec is %d\n",bursts_global[i].ts[0].tv_sec,bursts_global[i].ts[0].tv_usec);
            printf("last sec is %d,last usec is %d\n",bursts_global[i].ts[bursts_global[i].len-1].tv_sec,bursts_global[i].ts[bursts_global[i].len-1].tv_usec);
            printf("sec is %d,usec is %d\n",sec,usec);
            exit(-1);
        }
        printf("burst group %d , compute finished\n",i);
        /* //累加gap方式计算宽度，不准
        for(j=1;j<bursts_global[i].len-1;j++){
			bursts_global[i].width+=bursts_global[i].gaps[j];
		}
        */
        //bursts_global[i].width=
	}
}
/*
*打印结果
*/
void  print_result(){
    int i,j;
    for(i=0;i<ip_groups_global.len;i++){
        printf("burst gorup %d,ip is %d.%d.%d.%d , data is following:\n",
        i,bursts_global[i].src_ip >> 24 & 0xff,bursts_global[i].src_ip>>16 & 0xff,bursts_global[i].src_ip>>8 & 0xff,bursts_global[i].src_ip& 0xff);

        printf("burst width is %d ms\n",bursts_global[i].width);
        
        #ifdef PKT_GAP //计算波峰内包间隔部分，不需要时可注释
        for(j=1;j<=9;j++){
            printf("%d0p is %d us ,",j,(bursts_global[i].gaps[(int)((bursts_global[i].len-1)*0.1*j)]));
        }
        printf("99p is %d us\n",(bursts_global[i].gaps[(int)((bursts_global[i].len-1)*0.99)]));
        #endif
    }
}

void main(int argc,char** argv){
    char* file=argv[1];
    int i;
    ip_groups_global.len=0;
    for(i=0;i<10;i++){
        bursts_global[i].ts=(struct time_val *)malloc(sizeof(struct time_val)*1000000);
        if(bursts_global[i].ts==NULL){
            fprintf(stderr,"get mem fail,exit");
            exit(-1);
        }
        bursts_global[i].gaps=(int *)malloc(sizeof(int)*1000000);
        if(bursts_global[i].gaps==NULL){
            fprintf(stderr,"get mem fail,exit");
            exit(-1);
        }
        bursts_global[i].len=0;
		bursts_global[i].width=0;
    }
    process_pcap(file);
    compute_data();
    print_result();
    for(i=0;i<5;i++){
        free(bursts_global[i].ts);
        free(bursts_global[i].gaps);
    }
}

