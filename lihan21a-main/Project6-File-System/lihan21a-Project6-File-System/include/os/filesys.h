/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                           Copyright (C) 2023 UCAS
 *                Author : Cliver (email : lihan21a@mails.ucas.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                Interrupt processing related implementation
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#ifndef __INCLUDE_FILESYS_H__
#define __INCLUDE_FILESYS_H__

#include "os/kernel.h"
#include "pgtable.h"
#include <type.h>
#include "os/sched.h"
#include "pgtable.h"
#include <os/string.h>
#include <os/kernel.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <os/time.h>
#define SUPERBLOCK_MAGIC 0x20070225
#define NUM_FDESCS 16

#define FS_SIZE  (1 << 30)
#define FS_START (1 << 20) /*FS start block num*/

#define SECTOR_SIZE 512
#define BLOCK_SIZE 4096
//#define INODE_NUM 512           //num of inode
#define BLOCK_SUM_NUM (1 << 18)   //num of blocks 
//开1<<18个数据块，用block map,总共文件系统大小正好1个G

#define BLOCK_MAP_SECTOR_NUM 64    //block map占用的SECTOR数 BLOCK_SUM_NUM/(8*SECTOR_SIZE),32KB
#define INODE_MAP_SECTOR_NUM 1     //inode map占用的SECTOR数,
#define INODE_SECTOR_NUM     4096  //inode占用的SECTOR数,2MB

//super block用一个扇区
#define SUPERBLOCK_OFFSET  0
#define BLOCK_MAP_OFFSET  1  //
#define INODE_MAP_OFFSET  (BLOCK_MAP_OFFSET + BLOCK_MAP_SECTOR_NUM)
#define INODE_OFFSET      (INODE_MAP_OFFSET + INODE_MAP_SECTOR_NUM)
#define DATA_BLOCK_OFFSET (INODE_OFFSET + INODE_SECTOR_NUM)

#define SUPERBLOCK_ADDR    0x5d000000
#define BLOCK_MAP_ADDR     (SUPERBLOCK_ADDR + SECTOR_SIZE*BLOCK_MAP_OFFSET)
#define INODE_MAP_ADDR     (SUPERBLOCK_ADDR + SECTOR_SIZE*INODE_MAP_OFFSET)
#define INODE_ADDR         (SUPERBLOCK_ADDR + SECTOR_SIZE*INODE_OFFSET)
#define LEV_MEM_ADDR      (SUPERBLOCK_ADDR + SECTOR_SIZE*DATA_BLOCK_OFFSET)
#define DATA_BLOCK_ADDR    (LEV_MEM_ADDR   + SECTOR_SIZE*BLOCK_MAP_SECTOR_NUM)

#define IA_PER_BLOCK (BLOCK_SIZE/sizeof(uint32_t))
#define IA_PER_SECTOR (SECTOR_SIZE/sizeof(uint32_t))
#define DIRECT_SIZE (MAX_DIRECT_NUM*BLOCK_SIZE)
#define INDIRECT_1ST_SIZE (3*BLOCK_SIZE*IA_PER_BLOCK)
#define INDIRECT_2ND_SIZE (2*BLOCK_SIZE*IA_PER_BLOCK*IA_PER_BLOCK)
#define INDIRECT_3RD_SIZE (1*BLOCK_SIZE*IA_PER_BLOCK*IA_PER_BLOCK*IA_PER_BLOCK)
#define MAX_FILE_SIZE (DIRECT_SIZE + INDIRECT_1ST_SIZE + INDIRECT_2ND_SIZE + INDIRECT_3RD_SIZE)

#define MAX_FILE_NAME 100
#define MAX_DIRECT_NUM 10

enum File_type
{
    DIR,
    FILE
};

enum Inode_access
{
    R_A,
    W_A,
    RW_A
};

// modes of do_fopen 
enum Mode2fopen
{
    FOPEN_RDONLY = 1,
    FOPEN_WRONLY = 2,
    FOPEN_RDWR   = 3
};

// whence of do_lseek 
enum Lseek_whence
{
    SEEK_SET,
    SEEK_CUR,
    SEEK_END
};



typedef struct superblock_t{
    //[p6-task1]
    uint32_t magic;

    uint32_t fs_size;
    uint32_t fs_start;

    uint32_t blockmap_secnum; //blockmap占有的扇区数
    uint32_t blockmap_offset;

    uint32_t inodemap_secnum; //inodemap占有的扇区数
    uint32_t inodemap_offset;

    uint32_t inode_secnum; //inode占有的扇区数
    uint32_t inode_offset;

    uint32_t datablock_secnum; //block占有的扇区数
    uint32_t datablock_offset;

    uint32_t isize;
    uint32_t dsize;
} superblock_t;

typedef struct dir_entry_t{
    //p6-task1
    uint32_t mode;         //是文件还是目录
    uint32_t ino;    //iNode的number
    char name[MAX_FILE_NAME];
} dir_entry_t;

typedef struct inode_t{
    //p6 task1
    uint8_t mode;
    uint8_t access;   //权限，读 写 执行
    uint16_t link_count;
    uint32_t ino;
    uint32_t size;
    //
    uint32_t direct_ptr[MAX_DIRECT_NUM];
    uint32_t L1_ptr[3];
    uint32_t L2_ptr[2];
    uint32_t L3_ptr;

    uint32_t used_size;
    uint32_t modefy_time;
    uint32_t start_time;
    uint32_t aligned[4];
} inode_t;

typedef struct fdesc_t{
    // TODO [P6-task2]: Implement the data structure of file descriptor
    uint32_t ino;
    uint32_t r_ptr;
    uint32_t w_ptr;
    uint8_t mode;
} fdesc_t;

static char buffer[BLOCK_SIZE];
static char buffer2[BLOCK_SIZE];
static char level_buffer[3][BLOCK_SIZE];

/* fs function declarations */
extern int do_mkfs(void);
extern int do_statfs(void);
extern int do_cd(char *path);
extern int do_mkdir(char *path);
extern int do_rmdir(char *path);
extern int do_ls(char *path, int option);
extern int do_touch(char *path);
extern int do_cat(char *path);
extern int do_fopen(char *path, int mode);
extern int do_fread(int fd, char *buff, int length);
extern int do_fwrite(int fd, char *buff, int length);
extern int do_fclose(int fd);
extern int do_ln(char *src_path, char *dst_path);
extern int do_rm(char *path);
extern int do_lseek(int fd, int offset, int whence);

//清除一个扇区
static inline void clear_sector(uintptr_t mem_addr)
{
    int i = 0;
    for(i = 0; i < 512; i+=8)
    {
        *((uint64_t*)(mem_addr+i)) = 0;
    }
}

static inline void clear_block(uint32_t block_id)
{
    bios_sd_read(BLOCK_MAP_ADDR, 64, FS_START+BLOCK_MAP_OFFSET);
    uint8_t *bmap = (uint8_t*)pa2kva(BLOCK_MAP_ADDR);
    bmap[block_id/8] &= ~(1 << (block_id%8));
    bios_sd_write(BLOCK_MAP_ADDR, 64, FS_START+BLOCK_MAP_OFFSET);
}

static inline uint32_t alloc_data_block()
{
    bios_sd_read(BLOCK_MAP_ADDR, 64, FS_START+BLOCK_MAP_OFFSET);
    uint8_t *bmap = (uint8_t *)pa2kva(BLOCK_MAP_ADDR);
    uint32_t fb = 0;
    for(int i = 0; i < BLOCK_MAP_SECTOR_NUM*SECTOR_SIZE; i++)
    {
        for(int j = 0; j < 8; j++)
        {
            if(!(bmap[i] & (1<<j)))
            {
                fb = i*8+j;
                bmap[i] |= (1<<j);
                bios_sd_write(BLOCK_MAP_ADDR, 64, FS_START+BLOCK_MAP_OFFSET);
                return fb;
            }
        }
    }
    return 0;
}

static inline void clear_inode_map(uint32_t ino)
{
    bios_sd_read(INODE_MAP_ADDR, 1, FS_START+INODE_MAP_OFFSET);
    uint8_t *imap = (uint8_t*)pa2kva(INODE_MAP_ADDR);
    imap[ino/8] &= ~(1 << (ino%8));
    bios_sd_write(INODE_MAP_ADDR, 1, FS_START+INODE_MAP_OFFSET);
}

static inline uint32_t alloc_inode()
{
    bios_sd_read(INODE_MAP_ADDR, 1, FS_START+INODE_MAP_OFFSET);
    uint8_t *imap = (uint8_t*)pa2kva(INODE_MAP_ADDR);
    uint32_t ret_inode = 0;
    for(int i = 0; i < INODE_MAP_SECTOR_NUM*SECTOR_SIZE; i++)
    {
        for(int j = 0; j < 8; j++)
        {
            if(!(imap[i] & (1<<j)))
            {
                ret_inode = i*8+j;
                imap[i] |= (1<<j);
                bios_sd_write(INODE_MAP_ADDR, 1, FS_START+INODE_MAP_OFFSET);
                return ret_inode;
            }
        }
    }
    return 0;
}

static inline void write_inode(uint32_t ino)
{
    bios_sd_write(INODE_ADDR+ino*SECTOR_SIZE, 1, FS_START+INODE_OFFSET+ino);
}

static inline inode_t *get_inode(uint32_t ino)
{
    bios_sd_read(INODE_ADDR+ino*SECTOR_SIZE, 1, FS_START+INODE_OFFSET+ino);
    inode_t *inode = (inode_t*)pa2kva(INODE_ADDR+ino*SECTOR_SIZE);
    return inode;
}

static inline inode_t *lookup(inode_t *par_dp, char *name, int mode)
{
    bios_sd_read(DATA_BLOCK_ADDR, 8, FS_START + (par_dp->direct_ptr[0])*8);
    dir_entry_t *dir = (dir_entry_t*)pa2kva(DATA_BLOCK_ADDR);

    for(int i = 0; i < par_dp->used_size; i+=sizeof(dir_entry_t))
    {
        if((!strcmp(dir[i/sizeof(dir_entry_t)].name, name)))
        {
            if(dir[i/sizeof(dir_entry_t)].mode == mode)
            {
                inode_t *inode = get_inode(dir[i/sizeof(dir_entry_t)].ino);
                return inode;
            }
        }
    }
    return NULL;
}

static inline void print_superblock(superblock_t *sb)
{
    printk("\n");
    printk("[FileSystem] magic: 0x%x\n", sb -> magic);
    printk("[FileSystem] num sector: %d, start sector: %d\n",sb -> fs_size, sb -> fs_start);
    printk("[FileSystem] block map offset: %d occupied sector:%d\n", sb -> blockmap_offset, sb -> blockmap_secnum);
    printk("[FileSystem] inode map offset: %d occupied sector:%d\n", sb -> inodemap_offset, sb -> inodemap_secnum);
    printk("[FileSystem] inode offset: %d occupied sector:%d\n", sb -> inode_offset, sb -> inode_secnum);
    printk("[FileSystem] data offset: %d occupied sector:%d\n",sb -> datablock_offset, sb -> datablock_secnum);
    printk("[FileSystem] inode entry size: %dB, dir entry size: %dB\n", sb -> isize, sb -> dsize);
}

static inline void setup_level_index(uint32_t block_index, int level)
{
    uint32_t *addr_array = (uint32_t *)level_buffer[level];
    for(int i = 0; i < BLOCK_SIZE/sizeof(uint32_t); i++)
    {
        addr_array[i] = alloc_data_block();
    }
    if(level)
    {
        for(int i = 0; i < BLOCK_SIZE/sizeof(uint32_t); i++)
        {
            setup_level_index(addr_array[i], level-1);
        }
    }
    bios_sd_write(kva2pa((uint64_t)level_buffer[level]), 8, FS_START+block_index*8);
}

static inline uint32_t get_block_addr(inode_t inode, int size)
{
    //直接索引
    if(size < DIRECT_SIZE)
    {
        int ptr = size/BLOCK_SIZE;
        if(inode.direct_ptr[ptr] == 0)
            inode.direct_ptr[ptr] = alloc_data_block();
            
        return inode.direct_ptr[ptr];
    }
    //一级索引
    else if(size < DIRECT_SIZE + INDIRECT_1ST_SIZE)
    {
        size -= DIRECT_SIZE;
        int index1 = size/(BLOCK_SIZE*IA_PER_BLOCK);
        int index2 = (size - index1*IA_PER_BLOCK*BLOCK_SIZE)/BLOCK_SIZE;
        uint32_t temp_block;
        if(inode.L1_ptr[index1] == 0)
        {
            temp_block = alloc_data_block();
            setup_level_index(temp_block, 0);

            inode_t* inode_ptr = get_inode(inode.ino);
            inode_ptr->L1_ptr[index1] = temp_block;
            inode.L1_ptr[index1] = temp_block;
            write_inode(inode.ino);
        }
        else
        {
            temp_block = inode.L1_ptr[index1];
        }
        bios_sd_read(DATA_BLOCK_ADDR, 8, FS_START + 8*inode.L1_ptr[index1]);
        uint32_t* L0_ptr = (uint32_t*)(pa2kva(DATA_BLOCK_ADDR));
        return L0_ptr[index2];
    }
    //二级索引
    else if(size < DIRECT_SIZE + INDIRECT_1ST_SIZE + INDIRECT_2ND_SIZE)
    {
        size -= DIRECT_SIZE + INDIRECT_1ST_SIZE;
        int index1 = size/(BLOCK_SIZE*IA_PER_BLOCK*IA_PER_BLOCK);
        int index2 = (size - index1*IA_PER_BLOCK*BLOCK_SIZE*IA_PER_BLOCK)/(BLOCK_SIZE*IA_PER_BLOCK);
        int index3 = (size - index1*IA_PER_BLOCK*BLOCK_SIZE*IA_PER_BLOCK - index2*IA_PER_BLOCK)/BLOCK_SIZE;
        uint32_t temp_block;
        if(inode.L2_ptr[index1] == 0)
        {
            temp_block = alloc_data_block();
            setup_level_index(temp_block, 1);

            inode_t* inode_ptr = get_inode(inode.ino);
            inode_ptr->L2_ptr[index1] = temp_block;
            inode.L2_ptr[index1] = temp_block;
            write_inode(inode.ino);
        }
        else
        {
            temp_block = inode.L2_ptr[index1];
        }
        //获取二级索引项
        bios_sd_read(DATA_BLOCK_ADDR, 8, FS_START + inode.L2_ptr[index1]*8);
        uint32_t* L2_ptr = (uint32_t*)(pa2kva(DATA_BLOCK_ADDR));
        temp_block = L2_ptr[index2];
        //获取一级索引项
        bios_sd_read(DATA_BLOCK_ADDR, 8, FS_START + temp_block*8);
        uint32_t*L0_ptr = (uint32_t*)(pa2kva(DATA_BLOCK_ADDR));
        return L0_ptr[index3];
    }
    //3级索引
    else if(size < DIRECT_SIZE + INDIRECT_1ST_SIZE + INDIRECT_2ND_SIZE + INDIRECT_3RD_SIZE)
    {
        size -= (DIRECT_SIZE + INDIRECT_1ST_SIZE + INDIRECT_2ND_SIZE);
        int index1 = size/(BLOCK_SIZE*IA_PER_BLOCK*IA_PER_BLOCK);
        int index2 = (size - index1*BLOCK_SIZE*IA_PER_BLOCK*IA_PER_BLOCK)/(BLOCK_SIZE*IA_PER_BLOCK);
        uint32_t temp_block;
        if(inode.L3_ptr == 0)
        {
            temp_block = alloc_data_block();
            setup_level_index(temp_block, 1);
            inode_t* inode_ptr = get_inode(inode.ino);
            inode_ptr->L3_ptr = temp_block;
            inode.L3_ptr = temp_block;
            write_inode(inode.ino);
        }
        else
        {
            temp_block = inode.L3_ptr;
        }
        //获取二级索引项
        bios_sd_read(DATA_BLOCK_ADDR, 8, FS_START + inode.L3_ptr*8);
        uint32_t* L2_ptr = (uint32_t*)(pa2kva(DATA_BLOCK_ADDR));
        temp_block = L2_ptr[index1];
        //获取一级索引项
        bios_sd_read(DATA_BLOCK_ADDR, 8, FS_START + temp_block*8);
        uint32_t*L0_ptr = (uint32_t*)(pa2kva(DATA_BLOCK_ADDR));
        return L0_ptr[index2];
    }
    else 
    {
        printk("[FileSystem] File size too large!\n");
        return 0;
    }
}

static inline void recycle(uint32_t data_block, int level, void* zero_buff)
{
    if(level)
    {
        bios_sd_read(LEV_MEM_ADDR, 8, FS_START + data_block*8);
        uint32_t *L0_ptr = (uint32_t*)(pa2kva(LEV_MEM_ADDR));
        for(int i = 0; i < IA_PER_BLOCK; i++)
        {
            recycle(L0_ptr[i], level-1, zero_buff);
        }
    }

    bios_sd_write(kva2pa((uint64_t)zero_buff), 8, FS_START+data_block*8);
    clear_block(data_block);
}

static inline void recycle_level(uint32_t data_block, int level)
{
    bzero(buffer, BLOCK_SIZE);
    recycle(data_block, level, buffer);
}

#endif



