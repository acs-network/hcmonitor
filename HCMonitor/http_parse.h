#ifndef http_parser_h
#define http_parser_h

#include "rx_queue.h"

/* Request Methods */
#define GET 1
#define POST 2
#define HEAD 3
#define PUT 4
#define DELETE 5
#define TRACE 6

/* Response Methods */
#define OK 200

uint8_t http_parse(struct rte_ipv4_hdr *ip_hdr,struct rte_tcp_hdr *tcp,struct node_data *data,struct timespec ts_now,uint16_t payload_len);

uint8_t https_parse(struct rte_ipv4_hdr *ip_hdr,struct rte_tcp_hdr *tcp,struct node_data *data,struct timespec ts_now, uint16_t payload_len);

#endif
