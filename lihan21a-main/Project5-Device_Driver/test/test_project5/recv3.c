#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <syscall.h>

#define MAX_RECV_CNT 32
#define RX_PKT_SIZE 2000

uint8_t recv_buffer[MAX_RECV_CNT * RX_PKT_SIZE];

uint16_t fletcher16(uint8_t *data, int len)
{
    uint16_t sum1 = 0;
    uint16_t sum2 = 0;
    
    for(int i = 0; i < len; i++)
    {
        sum1 = (sum1 + data[i]) % 0xff;
        sum2 = (sum2 + sum1) % 0xff;
    }

    return (sum2 << 8) | sum1;
}
int main()
{
    int nbytes, checknum;
    
    nbytes = 28581;
    //nbytes = 50000;
    //while(1)
    //{
        sys_move_cursor(0, 2);
        printf("[RECV] start recv(%d): ", nbytes);

        for(int i = 0; i < nbytes; i++)
        {
            recv_buffer[i] = 0;
        }

        sys_net_recv_stream(recv_buffer, &nbytes);

        uint32_t size = (recv_buffer[3] << 24) |
                        (recv_buffer[2] << 16) |
                        (recv_buffer[1] << 8)  |
                        (recv_buffer[0]);
        sys_move_cursor(0, 3);
        printf("size = %d\n", size-4);

        // for(int i = 0; i < size - 4; i++)
        //     recv_buffer[i] = recv_buffer[i+4];

        printf("Received %d bytes\n", nbytes);
        printf("%c %c %c %c %c\n", recv_buffer[0+4], recv_buffer[1+4], recv_buffer[size-9+4],
                                      recv_buffer[size-7+4],recv_buffer[size-6+4]);

        // for(int i = 0; i < size - 4; i++)
        // {
        //     printf("%c", recv_buffer[i+4]);
        // }
        checknum = fletcher16((recv_buffer+4), size-4);
        printf("\nCHECKSUM: %d\n", checknum);
    //}
}
