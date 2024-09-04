#include "io.h"
#include "os/list.h"
#include <e1000.h>
#include <type.h>
#include <os/string.h>
#include <os/time.h>
#include <assert.h>
#include <pgtable.h>
#include <os/net.h>
#include <os/mm.h>

// E1000 Registers Base Pointer
volatile uint8_t *e1000;  // use virtual memory address

// E1000 Tx & Rx Descriptors
static struct e1000_tx_desc tx_desc_array[TXDESCS] __attribute__((aligned(16)));
static struct e1000_rx_desc rx_desc_array[RXDESCS] __attribute__((aligned(16)));

// E1000 Tx & Rx packet buffer
static char tx_pkt_buffer[TXDESCS][TX_PKT_SIZE];
static char rx_pkt_buffer[RXDESCS][RX_PKT_SIZE];

// Fixed Ethernet MAC Address of E1000
static const uint8_t enetaddr[6] = {0x00, 0x0a, 0x35, 0x00, 0x1e, 0x53};

/**
 * e1000_reset - Reset Tx and Rx Units; mask and clear all interrupts.
 **/
static void e1000_reset(void)
{
	/* Turn off the ethernet interface */
    e1000_write_reg(e1000, E1000_RCTL, 0);
    e1000_write_reg(e1000, E1000_TCTL, 0);

	/* Clear the transmit ring */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);

	/* Clear the receive ring */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, 0);

	/**
     * Delay to allow any outstanding PCI transactions to complete before
	 * resetting the device
	 */
    latency(1);

	/* Clear interrupt mask to stop board from generating interrupts */
    e1000_write_reg(e1000, E1000_IMC, 0xffffffff);

    /* Clear any pending interrupt events. */
    while (0 != e1000_read_reg(e1000, E1000_ICR)) ;
}

/**
 * e1000_configure_tx - Configure 8254x Transmit Unit after Reset
 **/
static void e1000_configure_tx(void)
{
    /* TODO: [p5-task1] Initialize tx descriptors */
    for(int i = 0; i < TXDESCS; i++)
    {
        tx_desc_array[i].addr = kva2pa((uintptr_t)tx_pkt_buffer[i]);
        tx_desc_array[i].status = E1000_TXD_STAT_DD;
        tx_desc_array[i].length = TX_PKT_SIZE;
        tx_desc_array[i].cmd = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;
    }

    /* TODO: [p5-task1] Set up the Tx descriptor base address and length */
    e1000_write_reg(e1000, E1000_TDBAL, (uint32_t)kva2pa((uintptr_t)tx_desc_array));
    e1000_write_reg(e1000, E1000_TDLEN, sizeof(tx_desc_array));

	/* TODO: [p5-task1] Set up the HW Tx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);

    /* TODO: [p5-task1] Program the Transmit Control Register */
    e1000_write_reg(e1000, E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP |
                                       E1000_TCTL_CT | E1000_TCTL_COLD);

    local_flush_dcache();
}

/**
 * e1000_configure_rx - Configure 8254x Receive Unit after Reset
 **/
static void e1000_configure_rx(void)
{
    /* TODO: [p5-task2] Set e1000 MAC Address to RAR[0] */
    // Fixed Ethernet MAC Address of E1000
    // static const uint8_t enetaddr[6] = {0x00, 0x0a, 0x35, 0x00, 0x1e, 0x53};
    e1000_write_reg_array(e1000, E1000_RA, 0, 0x00350a00);
    e1000_write_reg_array(e1000, E1000_RA, 1, 0x8000531e);

    /* TODO: [p5-task2] Initialize rx descriptors */
    for(int i = 0; i < RXDESCS; i++)
    {
        rx_desc_array[i].addr = kva2pa((uintptr_t)rx_pkt_buffer[i]);
    }

    /* TODO: [p5-task2] Set up the Rx descriptor base address and length */
    e1000_write_reg(e1000, E1000_RDBAL, (uint32_t)(kva2pa((uintptr_t)rx_desc_array)));
    e1000_write_reg(e1000, E1000_RDLEN, sizeof(rx_desc_array));

    /* TODO: [p5-task2] Set up the HW Rx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, RXDESCS-1);
    /* TODO: [p5-task2] Program the Receive Control Register */
    e1000_write_reg(e1000, E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM);

    /* TODO: [p5-task3] Enable RXDMT0 Interrupt */
    e1000_write_reg(e1000, E1000_IMS, E1000_IMS_RXDMT0);

    local_flush_dcache();
}

/**
 * e1000_init - Initialize e1000 device and descriptors
 **/
void e1000_init(void)
{
    /* Reset E1000 Tx & Rx Units; mask & clear all interrupts */
    e1000_reset();

    /* Configure E1000 Tx Unit */
    e1000_configure_tx();

    /* Configure E1000 Rx Unit */
    e1000_configure_rx();

    list_init(&send_block_queue);
    list_init(&recv_block_queue);
}

/**
 * e1000_transmit - Transmit packet through e1000 net device
 * @param txpacket - The buffer address of packet to be transmitted
 * @param length - Length of this packet
 * @return - Number of bytes that are transmitted successfully
 **/
int e1000_transmit(void *txpacket, int length)
{
    /* TODO: [p5-task1] Transmit one packet from txpacket */
    int tail;
    local_flush_dcache();
    tail = e1000_read_reg(e1000, E1000_TDT);

    assert(tx_desc_array[tail].status & E1000_TXD_STAT_DD);
    assert(length <= TX_PKT_SIZE);
    // if(!(tx_desc_array[tail].status & E1000_TXD_STAT_DD))
    //     return 0;

    for(int i = 0; i < length; i++)
    {
        tx_pkt_buffer[tail][i] = ((char *)txpacket)[i];
    }
    tx_desc_array[tail].length = length;
    tx_desc_array[tail].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
    tx_desc_array[tail].status = 0;

    //modify the TDT
    e1000_write_reg(e1000, E1000_TDT, (tail+1)%TXDESCS);
    local_flush_dcache();

    return length;
}

/**
 * e1000_poll - Receive packet through e1000 net device
 * @param rxbuffer - The address of buffer to store received packet
 * @return - Length of received packet
 **/
int e1000_poll(void *rxbuffer)
{
    /* TODO: [p5-task2] Receive one packet and put it into rxbuffer */
    int tail;
    local_flush_dcache();
    
    tail = e1000_read_reg(e1000, E1000_RDT);
    assert(rx_desc_array[(tail+1)%RXDESCS].status & E1000_RXD_STAT_DD);
    //assert(recvable(&tail));
    
    rx_desc_array[tail].status = 0;

    tail = (tail+1)%RXDESCS;
    int length = rx_desc_array[tail].length;
    for(int i = 0; i < length; i++)
    {
        ((char *)rxbuffer)[i] = rx_pkt_buffer[tail][i];
    }

    e1000_write_reg(e1000, E1000_RDT, tail);
    local_flush_dcache();
    return length;
}

int sendable(int *tailptr)
{
    local_flush_dcache();
    *tailptr = e1000_read_reg(e1000, E1000_TDT);
    return tx_desc_array[*tailptr].status & E1000_TXD_STAT_DD;
}

int recvable(int *tailptr)
{
    local_flush_dcache();
    *tailptr = e1000_read_reg(e1000, E1000_RDT);
    return rx_desc_array[(*tailptr+1)%RXDESCS].status & E1000_RXD_STAT_DD;
}

int e1000_poll_stream(void *rxbuffer, int *cur_seq, int nbytes, header_list_t *seq_node, char *tcp_header)
{
    local_flush_dcache();
    int tail = e1000_read_reg(e1000, E1000_RDT);
    assert(rx_desc_array[(tail+1)%RXDESCS].status & E1000_RXD_STAT_DD);

    tail =  (tail+1)%RXDESCS;
    for(int i = 0; i < TCP_IP_HEADER_LEN; i++)
    {
        ((char*)tcp_header)[i] = rx_pkt_buffer[tail][i];
    }

    uint16_t length = rx_desc_array[tail].length;
    uint8_t magic = rx_pkt_buffer[tail][TCP_IP_HEADER_LEN];
    uint8_t flags = rx_pkt_buffer[tail][TCP_IP_HEADER_LEN+1];
    uint16_t len = (rx_pkt_buffer[tail][TCP_IP_HEADER_LEN+2] << 8) |
                    rx_pkt_buffer[tail][TCP_IP_HEADER_LEN+3]; 
    uint32_t seq = (rx_pkt_buffer[tail][TCP_IP_HEADER_LEN+4] << 24) |
                   (rx_pkt_buffer[tail][TCP_IP_HEADER_LEN+5] << 16) |
                   (rx_pkt_buffer[tail][TCP_IP_HEADER_LEN+6] << 8) |
                    rx_pkt_buffer[tail][TCP_IP_HEADER_LEN+7];

    //序号超过最大限制，魔数非预期，序号比之前小
    if((seq > nbytes) || (magic != 0x45) || (seq < *cur_seq))
    {
        rx_desc_array[tail].status = 0;
        e1000_write_reg(e1000, E1000_RDT, tail);
        local_flush_dcache();
        return 0;
    }

    if(seq + len > nbytes)
    {
        len = nbytes - seq;
    }

    for(int i = 0; i < len; i++)
    {
        ((char*)rxbuffer)[seq+i] = rx_pkt_buffer[tail][TCP_IP_HEADER_LEN+8+i];
    }

    if(seq != *cur_seq)
    {
        header_list_t *temp_node = seq_node;
        while(temp_node->next != NULL)
        {
            if(seq < temp_node->next->seq)
                break;
            temp_node = temp_node->next;
        }
        header_list_t *new_node = (header_list_t *)kmalloc(sizeof(header_list_t));
        new_node->seq = seq;
        new_node->length = len;
        new_node->magic = magic;
        new_node->flags = flags;
        new_node->next = temp_node->next;
        temp_node->next = new_node;
    }
    else
    {
        *cur_seq = seq + len;
        header_list_t *temp_node = seq_node;
        while(temp_node->next != NULL)
        {
            if(temp_node->next->seq != *cur_seq)
                break;
            *cur_seq = temp_node->next->seq + temp_node->next->length;
            temp_node->next = temp_node->next->next;
        }
    }

    rx_desc_array[tail].status = 0;

    e1000_write_reg(e1000, E1000_RDT, tail);
    local_flush_dcache();

    return length;
}