#include <common.h>
#include <asm.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/loader.h>
#include <type.h>
#include "deflate/tinylibdeflate.h"


#define SECTOR_SIZE 512
#define KERNEL_ENTERPOINT 0x50201000

static void init_jmptab(void);

void main ()
{
    init_jmptab();
    int kernel_offset = *(int *)0x502001f2;
    int kernel_size = *(int *)0x502001ee;
    int kernel_blockid = kernel_offset / SECTOR_SIZE;
    int kernel_blocknum = *(short *)0x502001fc;

    /*先把压缩后的内核读到主存里*/
    bios_sd_read(KERNEL_ENTERPOINT, kernel_blocknum, kernel_blockid);
    memcpy((uint8_t *)KERNEL_ENTERPOINT, (uint8_t *)(KERNEL_ENTERPOINT + kernel_offset%SECTOR_SIZE), kernel_size);
    
    //memcpy((uint8_t *)enter_point, (uint8_t *)(enter_point + tasks[taskid].task_offset%SECTOR_SIZE), tasks[taskid].task_size);
    char *kennel_addr = KERNEL_ENTERPOINT;

    char origin_kernel[30*SECTOR_SIZE];
    char final_kernel[30*SECTOR_SIZE];
    int i = 0;
    for(i = 0; i < kernel_size; i++)
    {
        origin_kernel[i] = *(kennel_addr++);
    }

    /*解压内核*/
    kennel_addr = KERNEL_ENTERPOINT;
    int real_size = 0;
    struct libdeflate_decompressor * decompressor = deflate_alloc_decompressor();
    int mark = 0;
    mark = deflate_deflate_decompress(decompressor, origin_kernel, kernel_size, kennel_addr, 30*SECTOR_SIZE, &real_size);
    if(mark)
    {
        bios_putstr("An error occurred during decompression.\n");
        
    }

    /*解压后的内核搬到对应位置*/
/*    kennel_addr = KERNEL_ENTERPOINT;
    i = 0;
    for(i = 0; i < real_size; i++)
    {
        *(kennel_addr) = final_kernel[i];
        kennel_addr++;
    }*/
    bios_putstr("解压成功,普天同庆\n");
    asm volatile(
        "jalr %[addr]"
        ::[addr] "r" (KERNEL_ENTERPOINT)
    );
}

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
}