#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<netinet/in.h>
#include<time.h>
#include <malloc.h>  
# include <stdio.h>
# include <stdlib.h>
# include <stdlib.h>
/*
程序目标：
根据时间将pcap中的包分为多个聚合burst的pcap文件。

从第一个包开始依次向后遍历，出现第一个时间戳在一秒以后的数据包时，将从第一个包到这个包之间的数据包写入1s.pcap,以此类推。
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
int int_server_ip=0;
int times=1;
/*如果差距在600ms内，返回0 否则返回-1*/
int less_second(struct pcap_pkthdr * zero,struct pcap_pkthdr * current){
    int sec=current->ts.tv_sec-zero->ts.tv_sec;
    int usec=current->ts.tv_usec-zero->ts.tv_usec;
    //printf("times is %d current sec %d  current usec %d\n",times,current->ts.tv_sec,current->ts.tv_usec);
    //printf("times is %d current sec %d  current usec %d\n",times,zero->ts.tv_sec,zero->ts.tv_usec);
    if(sec==0 && (usec/1000)<=700){
        return 0;
    }
    else if(sec==1 && (usec/1000)<=-300){
        return 0;
    }
    else{
        return -1;
    }
}


/*
* 函数定义
* pcap遍历处理
*     根据每个数据包的时间戳，将该数据包内所需数据发送

*/

void process_pcap(char* file){
    struct pcap_file_header *file_header,*file_header_0;
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
    int burst_seq=1;
    //初始化
    file_header= (struct pcap_file_header *)malloc(sizeof(struct pcap_file_header));
    file_header_0= (struct pcap_file_header *)malloc(sizeof(struct pcap_file_header));
    pkt_header = (struct pcap_pkthdr *)malloc(sizeof(struct pcap_pkthdr));
    pkt_header_0 = (struct pcap_pkthdr *)malloc(sizeof(struct pcap_pkthdr));
	eth_header= (struct rte_ether_hdr *)malloc(sizeof(struct rte_ether_hdr));
    ip_header=(struct rte_ipv4_hdr *)malloc(sizeof(struct rte_ipv4_hdr));
    memset(pkt_header_0, 0, sizeof(struct pcap_pkthdr));
    if ((fp = fopen(file, "r")) == NULL)
    {
        printf("error: can not open pcap file\n");
        exit(0);
    }

    //创建client.pcap
    char current_file[100]="1s.pcap";
    //current_file = (char *)malloc(100);
    //current_file="1s.pcap";
    FILE *current_output = fopen(current_file,"w+");
	if(current_output == 0)
	{
		printf( "%d creat client pcap fail!",__LINE__);
		exit(-1);
	}
    fread(file_header,24,1,fp);
    *file_header_0=*file_header;
    fwrite(file_header,24,1,current_output);
    //开始读数据包
    pkt_offset = 24; //pcap文件头结构 24个字节
    while (fseek(fp, pkt_offset, SEEK_SET) == 0) //遍历数据包
    {
        //pcap_pkt_header 16 byte
        //pkt_header_last=pkt_header;
		memset(pkt_header, 0, sizeof(struct pcap_pkthdr));
		int tmp_offset=0;
        if (fread(pkt_header, 16, 1, fp) != 1) //读pcap数据包头结构
        {
            printf("\nread end of pcap file\n");
            break;
        }
        tmp_offset += 16;
        pkt_offset += 16 + pkt_header->caplen;   //下一个数据包的偏移值
		if(i==0){
            *pkt_header_0=*pkt_header;
        }

		//数据帧头 14字节
        memset(eth_header, 0, sizeof(struct rte_ether_hdr));
        if (fread(eth_header, sizeof(struct rte_ether_hdr), 1, fp) != 1)
        {
            printf("%d: can not read ip_header\n", i);
            continue;
        }
        tmp_offset+=sizeof(struct rte_ether_hdr);
        //忽略vlan头
        if(ntohs(eth_header->ether_type)==RTE_ETHER_TYPE_VLAN){
            fseek(fp, 4, SEEK_CUR);
            tmp_offset+=4;
        }
        //IP数据报头 20字节
        memset(ip_header, 0, sizeof(struct rte_ipv4_hdr));
        if (fread(ip_header, sizeof(struct rte_ipv4_hdr), 1, fp) != 1)
        {
            printf("%d: can not read ip_header\n", i);
            continue;
        }
        tmp_offset+=sizeof(struct rte_ipv4_hdr);
        //add(ntohl(ip_header->src_addr),pkt_header->ts,pkt_header_last->ts);
        if(less_second(pkt_header_0,pkt_header)==0){//一秒内
            char cs[16+pkt_header->caplen];
            fseek(fp, -tmp_offset, SEEK_CUR);
            fread(cs,(16+pkt_header->caplen),1,fp);
            fwrite(cs,(16+pkt_header->caplen),1,current_output);
        }
        else{
            times++;
            //if(times>=4) return; 
            fclose(current_output);
            strcpy(current_file,"");  
            char str[10]=""; 
            //itoa (times, str, 10);
            sprintf(str, "%d", times);
            strcat(current_file,str);
            strcat(current_file,"s.pcap");

            current_output = fopen(current_file,"w+");
	        if(current_output == 0)
	        {
		        printf( "%d creat client pcap fail!",__LINE__);
		        exit(-1);
	        }
            fwrite(file_header_0,24,1,current_output);
            char cs[16+pkt_header->caplen];
            fseek(fp, -tmp_offset, SEEK_CUR);
            fread(cs,(16+pkt_header->caplen),1,fp);
            fwrite(cs,(16+pkt_header->caplen),1,current_output);
            *pkt_header_0=*pkt_header;
        }
        i++;
    } // end while
    fclose(fp);
    fclose(current_output);
    free(pkt_header);
    free(file_header_0);
    free(pkt_header);
    free(pkt_header_0);
    free(eth_header);
    free(ip_header);
}

void main(int argc,char** argv){
    char* file=argv[1];
    process_pcap(file);
}

