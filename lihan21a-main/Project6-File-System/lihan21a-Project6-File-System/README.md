# Project6-文件系统

## 实验任务
- 任务一 实现网卡初始化与轮询发送数据帧的功能。
- 任务二 实现网卡初始化与轮询接收数据帧的功能。
- 任务三 添加网卡中断，实现基于网卡中断的收发数据帧的功能。
- 任务四 可靠的网络数据传输


## 运行方式
```sh
make all     #编译文件，制作image文件
make run     #启动qemu，运行
make run-smp #启动qemu，运行，开启多核
#上板运行
make floppy  #把镜像写到SD卡里
make minicom #监视串口
```

进入BBL界面之后，输入```loadbootd```启动单核，输入```loadbootm```启动双核，shell界面如下。
使用下述命令来抓取qemu上发送的包：

```sh
sudo tcpdump -i tap0 -XX -vvv -nn
```

使用`pktRxTx`程序来测试收发包。


## 设计流程
### 1. 初始化收发包的buffer
- 这部分工作在`e1000_configure_tx`和``e1000_configure_rx``里完成
- 定义两个管理收发包的数据结构，每个数据结构有与之相对应的buffer，地址设置成buffer的地址
- 初始化数据结构的其他内容，包括长度，状态，cmd等
- 建立基地址和长度，并初始化头指针和尾指针

### 2. 收包操作
- 收包前判断此时的状态，如果不可收就将当前进程block
- 调用`e1000_transmit`来接收数据，将数据放入buffer，然后更新长度、状态
  
### 3. 中断操作
- 在中断处理程序中加入处理收发包中断的入口`handle_irq_ext`,分别判断当前能不能收或者发
- 如果可以收或者发，就从对应的阻塞队列中unblock出来一个进程


### 4.可靠传输
- 发包乱序，选择用链表处理seq,定义指针cur_seq
- 在判断可以收包后，通过`e1000_poll_stream`收包，解析出发来数据的seq等
- 对比这个seq，如果与cur_seq一致，说明可以确认收到，更新seq，如果比cur_seq大，说明数据包乱序，需要等待,做成链表放入
- 在外面检测定义pre_seq，每次检查两者是否一致，如果不一致说明确认到新的包，发ACK
- 长时间没有收到就发RSD，每次发包都要重置RSD的时间
- 设置unblock的时间，每次进程调度检查是否有阻塞在recv_block_queue的进程到时间，有就unblock


## 关键代码
1）e1000_transmit(在e1000.c) 
```c
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
```

2）2）e1000_poll(e1000.c中)
```c
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
```

3）e1000_poll_stream(e1000.c中)
```c
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
```

## 实验总结
整体难度不大，但是小程序的使用还是有些问题。在Ccore中，有时候小程序会卡住，这时候用tcpdump抓包，发现操作系统一直在发RSD请求，但小程序没有反应。并且不止一个同学出现了这个问题，最终没能很好的解决。