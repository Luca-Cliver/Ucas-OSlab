#include "io.h"
#include <assert.h>
#include <e1000.h>
#include <type.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/list.h>
#include <os/smp.h>
#include <os/net.h>

// static LIST_HEAD(send_block_queue);
// static LIST_HEAD(recv_block_queue);

int do_net_send(void *txpacket, int length)
{
    uint64_t cpu_id = get_current_cpu_id();
    // TODO: [p5-task1] Transmit one network packet via e1000 device
    // while(!e1000_transmit(txpacket, length))
    // {
    //     ;
    // }
    //assert(length);
    // TODO: [p5-task3] Call do_block when e1000 transmit queue is full
    // TODO: [p5-task3] Enable TXQE interrupt if transmit queue is full
    int tail;
    if(!sendable(&tail))
    {
        e1000_write_reg(e1000, E1000_IMS, E1000_IMS_TXQE);
        local_flush_dcache();

        while(!sendable(&tail))
        {
            time2unblock_net = (uint64_t)get_timer() + TIME_UNBLOCK;
            do_block(&current_running[cpu_id]->list, &send_block_queue);
        }
        if(list_is_empty(&send_block_queue))
        {
            e1000_write_reg(e1000, E1000_IMC, E1000_IMC_TXQE);
        }

    }

    length = e1000_transmit(txpacket, length);

    return length;  // Bytes it has transmitted
}

int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    // TODO: [p5-task2] Receive one network packet via e1000 device
    // int total_length = 0;
    // for(int i = 0; i < pkt_num; i++)
    // {
    //     while((pkt_lens[i] = e1000_poll(rxbuffer + total_length)) == 0)
    //     {
    //         ;
    //     }
    //     //assert(pkt_lens[i]);
    //     total_length += pkt_lens[i];
    // }
    
    // TODO: [p5-task3] Call do_block when there is no packet on the way
    int total_length = 0;
    for(int i = 0; i < pkt_num; i++)
    {
        int tail;
        while(!recvable(&tail))
        {   //printk("to be blocked, loc 65");
            do_block(&current_running[get_current_cpu_id()]->list, &recv_block_queue);
        }

        pkt_lens[i] = e1000_poll(rxbuffer + total_length);
        total_length += pkt_lens[i];
    }
    return total_length;  // Bytes it has received
}

void net_handle_irq(void)
{
    // TODO: [p5-task3] Handle interrupts from network device
    //printk("\nRDH %d RDT %d\n", e1000_read_reg(e1000,E1000_RDH), e1000_read_reg(e1000,E1000_RDT));
    uint64_t irq_cause = e1000_read_reg(e1000, E1000_ICR);
    if(irq_cause & E1000_ICR_TXQE)
    {
        //printk("receiving,loc:82\n");
        int tail;
        if(!list_is_empty(&send_block_queue))
        {
            if(sendable(&tail))
            {
                //printk("receiving,loc:88\n");
                do_unblock(send_block_queue.next);
            }
            else
            {
                printk("nothing to send\n");
            }
        }
    }

    if(irq_cause & E1000_ICR_RXDMT0)
    {
        //printk("receiving,loc:99\n");
        int tail;
        if(!list_is_empty(&recv_block_queue))
        {
            if(recvable(&tail))
            {
                //printk("receiving,loc:105\n");
                do_unblock(recv_block_queue.next);
            }
            else
            {
                printk("nothing to recv\n");
            }
        }
    }
}