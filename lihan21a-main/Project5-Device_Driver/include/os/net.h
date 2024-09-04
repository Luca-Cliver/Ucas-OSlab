#ifndef __INCLUDE_NET_H__
#define __INCLUDE_NET_H__

#include <os/list.h>
#include <type.h>

#define PKT_NUM 32
#define TIME_RESEND 50000
//#define TIME_RESEND 2
#define TIME_UNBLOCK 1
#define TCP_IP_HEADER_LEN 54

void net_handle_irq(void);
int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens);
int do_net_send(void *txpacket, int length);
int do_net_recv_stream(void *rxbuffer, int *nbytes);

void do_send_rsd(int seq, char *tcp_header);
void do_send_ack(int seq, char *tcp_header);

void check_net_unblock(void);

extern uint64_t time2resend;
extern uint64_t time2unblock_net;


static LIST_HEAD(send_block_queue);
static LIST_HEAD(recv_block_queue);


#endif  // !__INCLUDE_NET_H__