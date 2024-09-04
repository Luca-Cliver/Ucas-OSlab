#include "io.h"
#include <assert.h>
#include <e1000.h>
#include <type.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/list.h>
#include <os/smp.h>
#include <os/net.h>

/* Mask of the `flag` byte in header */
#define FLAG_ACK (1U << 2)
#define FLAG_RSD (1U << 1)
#define FLAG_DAT (1U << 0)

uint64_t time2resend;
uint64_t time2unblock_net;

int do_net_recv_stream(void *buffer, int *nbytes)
{
    //uint64_t cpu_id = get_current_cpu_id();
    char tcp_header[TCP_IP_HEADER_LEN];
    int cur_seq = 0, pre_seq = 0, inorder_time = 0;
    time2resend = get_ticks() + TIME_RESEND;
    time2unblock_net = get_timer() + TIME_UNBLOCK;
    header_list_t *seq_node = (header_list_t *)kmalloc(sizeof(header_list_t));
    seq_node->next = NULL;
    seq_node->length = -1;
    seq_node->seq = -1;
    seq_node->magic = -1;
    seq_node->flags = -1;

    while(1)
    {
        int tail;
        while(!recvable(&tail))
        {   
            time2unblock_net = get_timer() + TIME_UNBLOCK;
            do_block(&current_running[get_current_cpu_id()]->list, &recv_block_queue);
            if(get_ticks() >= time2resend && !recvable(&tail))
            {
                //send rsd, reset time
                time2resend = get_ticks() + TIME_RESEND;
                inorder_time = 0;
                do_send_rsd(cur_seq, &tcp_header[0]);
            }
        }

        int temp_len = e1000_poll_stream(buffer, &cur_seq, *nbytes, seq_node, tcp_header);
        if(temp_len == 0)
            continue;
        if(cur_seq != pre_seq)
        {
            pre_seq = cur_seq;
            inorder_time = 0;
            time2resend = get_ticks() + TIME_RESEND;
            do_send_ack(cur_seq, tcp_header);
        }
        else
        {
            if(inorder_time < /*TIME_RESEND*/3)
            {
                inorder_time++;
                if(get_ticks() >= time2resend)
                {
                    time2resend = get_ticks() + TIME_RESEND;
                    inorder_time = 0;
                    do_send_rsd(cur_seq, tcp_header);
                }
            }
            else
            {
                inorder_time = 0;
                do_send_rsd(cur_seq, tcp_header);
            }   
            // if(get_ticks() >= time2resend)
            // {
            //     time2resend = get_ticks() + TIME_RESEND;
            //     inorder_time = 0;
            //     do_send_rsd(cur_seq, tcp_header);
            // }
        }
        if(cur_seq == *nbytes)
            break;
    }
    return 1;
}

void do_send_rsd(int seq, char *tcp_header)
{
    char rsd_buffer[TCP_IP_HEADER_LEN+8];
    memcpy((uint8_t *)rsd_buffer, (uint8_t *)tcp_header, TCP_IP_HEADER_LEN);
    rsd_buffer[TCP_IP_HEADER_LEN] = 0x45;
    rsd_buffer[TCP_IP_HEADER_LEN+1] = FLAG_RSD;
    rsd_buffer[TCP_IP_HEADER_LEN+2] = 0;
    rsd_buffer[TCP_IP_HEADER_LEN+3] = 0;

    for(int i = 3; i >= 0; i--)
    {
        rsd_buffer[TCP_IP_HEADER_LEN+4+i] = seq & 0xffU;
        seq = seq >> 8;
    }
    do_net_send((void *)rsd_buffer, TCP_IP_HEADER_LEN+8);
    
}

void do_send_ack(int seq, char *tcp_header)
{
    char ack_buffer[TCP_IP_HEADER_LEN+8];
    memcpy((uint8_t *)ack_buffer, (uint8_t *)tcp_header, TCP_IP_HEADER_LEN);
    ack_buffer[TCP_IP_HEADER_LEN] = 0x45;
    ack_buffer[TCP_IP_HEADER_LEN+1] = FLAG_ACK;
    ack_buffer[TCP_IP_HEADER_LEN+2] = 0;
    ack_buffer[TCP_IP_HEADER_LEN+3] = 0;

    for(int i = 3; i >= 0; i--)
    {
        ack_buffer[TCP_IP_HEADER_LEN+4+i] = seq & 0xffU;
        seq = seq >> 8;
    }

    do_net_send((void *)ack_buffer, TCP_IP_HEADER_LEN+8);
}

void check_net_unblock()
{
    if(get_timer() >= time2unblock_net)
    {
        time2unblock_net = get_timer() + TIME_UNBLOCK;
        if(!list_is_empty(&recv_block_queue))
        {
            do_unblock(recv_block_queue.next);
        }
        if(!list_is_empty(&send_block_queue))
        {
            do_unblock(send_block_queue.next);
        }
    }
}
