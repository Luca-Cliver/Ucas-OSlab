#include "os/sched.h"
#include "pgtable.h"
#include "type.h"
#include <os/string.h>
#include <os/filesys.h>
#include <os/kernel.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <os/time.h>


static superblock_t superblock;
static fdesc_t fdesc_array[NUM_FDESCS];
inode_t *current_inode;
int state = 0;

//初始化目录
void init_dir_entry(uint32_t block_num, uint32_t ino, uint32_t pino)
{
    dir_entry_t *dir = (dir_entry_t*)(pa2kva(DATA_BLOCK_ADDR));
    clear_sector((uintptr_t)dir);

    //目录本身
    dir[0].mode = DIR;
    dir[0].ino = ino;
    strcpy(dir[0].name, ".");

    //上级目录
    dir[1].mode = DIR;
    dir[1].ino = pino;
    strcpy(dir[1].name, "..");

    bios_sd_write(DATA_BLOCK_ADDR, 8, FS_START+block_num*8);
}

//解析路径
int find = 0;
int depth = 0;
int find_dir(char *path, int mode)
{
    depth = 0;
    int i = 0;
    if(depth == 0){
        find = 0;
    }
    
    if(path[0] == '/')
    {
        bios_sd_read(INODE_ADDR, 1, FS_START+INODE_OFFSET);
        current_inode = (inode_t*)(pa2kva(INODE_ADDR));
        if(path[1] == '\0')
        {
            return 1;
        }
        for(i = 0; i < strlen(path); i++)
        {
            path[i] = path[i+1];
        }
        path[i] = '\0';
    }

    char head[20], tail[100];

    for(i = 0; i < strlen(path) && path[i] != '/'; i++)
    {
        head[i] = path[i];
    }
    head[i++] = '\0';

    int j = 0;
    for(j = 0; i < strlen(path); j++, i++)
    {
        tail[j] = path[i];
    }
    tail[j] = '\0';

    inode_t *find_inode;
    if((find_inode = lookup(current_inode, head, mode)) != 0)
    {
        depth++;
        current_inode = find_inode;
        if(tail[0] == '\0')
        {
            find = 1;
            return 1;
        }
        find_dir(tail, mode);
    }
    if(find == 1)
    {
        depth--;
        return 1;
    }
    else 
    {
        depth = 0;
        return 0;
    }
}

int do_mkfs(void)
{
    bios_sd_read(SUPERBLOCK_ADDR, 1, FS_START);
    superblock_t *sb = (superblock_t*)pa2kva(SUPERBLOCK_ADDR);
    screen_reflush();

    printk("[FileSystem] initializing filesystem!\n");

    //初始化superblock
    printk("[FileSystem] initializing superblock!\n");
    clear_sector((uintptr_t)sb);
    //先初始化数据结构
    sb->magic = SUPERBLOCK_MAGIC;
    sb->fs_size = FS_SIZE;
    sb->fs_start = FS_START;
    sb->blockmap_secnum = BLOCK_MAP_SECTOR_NUM;
    sb->blockmap_offset = BLOCK_MAP_OFFSET;
    sb->inodemap_secnum = INODE_MAP_SECTOR_NUM;
    sb->inodemap_offset = INODE_MAP_OFFSET;
    sb->inode_secnum = INODE_SECTOR_NUM;
    sb->inode_offset = INODE_OFFSET;
    sb->datablock_secnum = (FS_SIZE/SECTOR_SIZE) - DATA_BLOCK_OFFSET;
    sb->datablock_offset = DATA_BLOCK_OFFSET;
    sb->isize = sizeof(inode_t);
    sb->dsize = sizeof(dir_entry_t);
    print_superblock(sb);

    //建立block级映射
    printk("[FileSystem] create block map\n");
    uint8_t *bmap = (uint8_t *)(pa2kva(BLOCK_MAP_ADDR));
    for(int i = 0; i < sb->blockmap_secnum; i++)
    {
        clear_sector((uintptr_t)(bmap + i * SECTOR_SIZE));
    }
    for(int i = 0; i < (sb->datablock_offset)/8 + 1; i++)
    {
        bmap[i/8] |= (1 << (i%8));
    }
    //建立inode映射
    printk("[FileSystem] create inode map\n");
    uint8_t *imap = (uint8_t *)(pa2kva(INODE_MAP_ADDR));
    clear_sector((uintptr_t)imap);
    imap[0] = 0x01;
    bios_sd_write(SUPERBLOCK_ADDR, 66, FS_START);
    bios_sd_read(SUPERBLOCK_ADDR, 1, FS_START);
    superblock_t *test_sb = (superblock_t*)pa2kva(SUPERBLOCK_ADDR);
    //建立inode
    printk("[FileSystem] create inode\n");
    inode_t *root_inode = (inode_t *)(pa2kva(INODE_ADDR));
    root_inode[0].ino = 0;
    root_inode[0].mode = DIR;
    root_inode[0].access = RW_A;
    root_inode[0].link_count = 0;
    root_inode[0].size = BLOCK_SIZE;
    root_inode[0].used_size = 2*sizeof(dir_entry_t);     //两个dir .和..
    root_inode[0].start_time = get_timer();
    root_inode[0].modefy_time = get_timer();
    root_inode[0].L1_ptr[0] = 0;
    root_inode[0].L1_ptr[1] = 0;
    root_inode[0].L1_ptr[2] = 0;
    root_inode[0].L2_ptr[0] = 0;
    root_inode[0].L2_ptr[1] = 0;
    root_inode[0].L3_ptr = 0;
    root_inode[0].direct_ptr[0] = alloc_data_block();
    for(int i = 1; i < MAX_DIRECT_NUM; i++)
    {
        root_inode[0].direct_ptr[i] = 0;
    }
    write_inode(root_inode->ino);

    //建立目录项
    printk("[FileSystem] create dir entry\n");
    init_dir_entry(root_inode[0].direct_ptr[0], root_inode[0].ino, 0);

    printk("[FileSystem] Initializing filesystem finished!\n");

    current_inode = root_inode;
    return 0;
}

int do_statfs(void)
{
    bios_sd_read(SUPERBLOCK_ADDR, 1, FS_START);
    superblock_t *sb = (superblock_t *)pa2kva(SUPERBLOCK_ADDR);
    //检查sb
    if(sb->magic != SUPERBLOCK_MAGIC)
    {
        printk("[FileSystem] Superblock magic error!\n");
        return -1;
    }

    int used_block_num = 0;
    int used_inode_num = 0;

    //统计已使用的块和inode
    bios_sd_read(BLOCK_MAP_ADDR, BLOCK_MAP_SECTOR_NUM, FS_START+BLOCK_MAP_OFFSET);
    uint8_t *bmap = (uint8_t *)pa2kva(BLOCK_MAP_ADDR);
    for(int i = 0; i < BLOCK_MAP_SECTOR_NUM*SECTOR_SIZE; i++)
    {
        for(int j = 0; j < 8; j++)
        {
            used_block_num += (bmap[i] >> j) & 1;
        }
    }
    bios_sd_read(INODE_MAP_ADDR, 1, FS_START+INODE_MAP_OFFSET);
    uint8_t *imap = (uint8_t *)pa2kva(INODE_MAP_ADDR);
    for(int i = 0; i < INODE_MAP_SECTOR_NUM*SECTOR_SIZE; i++)
    {
        for(int j = 0; j < 8; j++)
        {
            used_inode_num += (imap[i] >> j) & 1;
        }
    }

    printk("[FileSystem] magic: 0x%x\n", sb -> magic);
    printk("[FileSystem] used block: %d/%d, start sector: %d(0x%x)\n",used_block_num, FS_SIZE, FS_START, FS_START);
    printk("[FileSystem] block map offset: %d occupied sector:%d\n", sb -> blockmap_offset, sb -> blockmap_secnum);
    printk("[FileSystem] inode map offset: %d occupied sector:%d used: %d/4096\n", sb -> inodemap_offset, sb -> inodemap_secnum,used_inode_num);
    printk("[FileSystem] inode offset: %d occupied sector:%d\n", sb -> inode_offset, sb -> inode_secnum);
    printk("[FileSystem] data offset: %d occupied sector:%d\n",sb -> datablock_offset, sb -> datablock_secnum);
    printk("[FileSystem] inode entry size: %dB, dir entry size: %dB\n", sb -> isize, sb -> dsize);

    return 0;
}

int do_cd(char *path)
{
    bios_sd_read(SUPERBLOCK_ADDR, 1, FS_START);
    superblock_t *sb = (superblock_t *)pa2kva(SUPERBLOCK_ADDR);
    if(sb->magic != SUPERBLOCK_MAGIC)
    {
        printk("[FileSystem] magic error\n");
        return 0;
    }

    bios_sd_read(INODE_ADDR, 1, FS_START+INODE_OFFSET);
    if(state == 0)
    {
        current_inode = (inode_t *)pa2kva(INODE_ADDR);
        state = 1;
    }

    inode_t *cur_inode = current_inode;
    if(path[0] != '\0')
    {
        if(find_dir(path, DIR) == 0)
        {
            current_inode = cur_inode;
            printk("[FileSystem] cd error\n");
            return 0;
        }
    }
    if(current_inode->mode == FILE)
    {
        current_inode = cur_inode;
        printk("[FileSystem] cd error,can not cd in a file\n");
    }
    return 1;
}

int do_mkdir(char *path)
{
    bios_sd_read(SUPERBLOCK_ADDR, 1, FS_START);

    superblock_t *sb = (superblock_t *)pa2kva(SUPERBLOCK_ADDR);
    if(sb->magic != SUPERBLOCK_MAGIC)
    {
        printk("[FileSystem] magic error\n");
        return -1;
    }

    bios_sd_read(INODE_ADDR, 1, FS_START+INODE_OFFSET);
    if(state == 0)
    {
        current_inode = (inode_t *)pa2kva(INODE_ADDR);
        state = 1;
    }

    //检查这个目录是否存在
    inode_t *par_dir = current_inode;
    inode_t *find_dir = lookup(par_dir, path, DIR);
    if(find_dir != NULL)
    {
        printk("[FileSystem] mkdir error,the dir is already exist\n");
        return -1;
    }
    //分配inode
    uint32_t inode_num = alloc_inode();
    if(inode_num == 0)
    {
        printk("[FileSystem] mkdir error,no inode\n");
        return -1;
    }
    inode_t *new_inode = (inode_t *)pa2kva(INODE_ADDR+inode_num*SECTOR_SIZE);
    new_inode->ino = inode_num;
    new_inode->mode = DIR;
    new_inode->access = RW_A;
    new_inode->link_count = 0;
    new_inode->size = BLOCK_SIZE;
    new_inode->used_size = 2*sizeof(dir_entry_t); //.和..
    new_inode->start_time = get_timer();
    new_inode->modefy_time= get_timer();
    new_inode->direct_ptr[0] = alloc_data_block();
    for(int i = 1; i < MAX_DIRECT_NUM; i++)
    {
        new_inode->direct_ptr[i] = 0;
    }
    new_inode->L1_ptr[0] = 0;
    new_inode->L1_ptr[1] = 0;
    new_inode->L1_ptr[2] = 0;
    new_inode->L2_ptr[0] = 0;
    new_inode->L2_ptr[1] = 0;
    new_inode->L3_ptr = 0;
    write_inode(new_inode->ino);

    //做目录
    init_dir_entry(new_inode->direct_ptr[0], new_inode->ino, par_dir->ino);
    //加目录到其父目录
    bios_sd_read(DATA_BLOCK_ADDR, 8, FS_START+(par_dir->direct_ptr[0]*8));
    dir_entry_t *dir_entry = (dir_entry_t *)(pa2kva(DATA_BLOCK_ADDR)+par_dir->used_size); //找到没有用过的地址
    dir_entry->ino = new_inode->ino;
    dir_entry->mode = DIR;
    strcpy(dir_entry->name, path);
    bios_sd_write(DATA_BLOCK_ADDR, 8, FS_START+(par_dir->direct_ptr[0]*8));
    //更新父inode
    par_dir->modefy_time = get_timer();
    par_dir->link_count++;
    par_dir->used_size += sizeof(dir_entry_t);
    write_inode(par_dir->ino);

    printk("[FileSystem] Successfully make a dir\n");
    return 0;
}

int do_rmdir(char *path)
{
    bios_sd_read(SUPERBLOCK_ADDR, 1, FS_START);
    superblock_t *sb = (superblock_t *)pa2kva(SUPERBLOCK_ADDR);
    if(sb->magic != SUPERBLOCK_MAGIC)
    {
        printk("[FileSystem] Superblock magic error");
        return -1;
    }

    bios_sd_read(INODE_ADDR, 1, FS_START+INODE_OFFSET);
    if(state == 0)
    {
        current_inode = (inode_t *)(pa2kva(INODE_ADDR));
        state = 1;
    }

    inode_t *par_inode = current_inode;
    inode_t *find_dir = lookup(par_inode, path, DIR);

    if(find_dir == NULL)
    {
        printk("[FileSystem] Can't find the dir");
        return -1;
    }

    //清除映射
    clear_block(find_dir->direct_ptr[0]);
    clear_inode_map(find_dir->ino);

    //删除文件
    bios_sd_read(DATA_BLOCK_ADDR, 8, FS_START + (par_inode->direct_ptr[0]*8));
    dir_entry_t *dir = (dir_entry_t *)(pa2kva(DATA_BLOCK_ADDR));
    int found = 0;
    for(int i = 0; i < par_inode->used_size; i+=sizeof(dir_entry_t))
    {
        if(found)
        {
            memcpy((uint8_t*)(dir-1), (uint8_t*)dir, sizeof(dir_entry_t));
        }
        if(find_dir->ino == dir->ino)
        {
            found = 1;
        }
        dir++;
    }
    bios_sd_write(DATA_BLOCK_ADDR, 8, FS_START + (par_inode->direct_ptr[0]*8));

    bios_sd_read(DATA_BLOCK_ADDR, 8, FS_START+(find_dir->direct_ptr[0]*8));
    current_inode = find_dir;
    dir_entry_t *rm_dir = (dir_entry_t *)(pa2kva(DATA_BLOCK_ADDR));
    for(int i = 2*sizeof(dir_entry_t); i < find_dir->used_size; i+=sizeof(dir_entry_t))
    {
        // if(rm_dir[i/sizeof(dir_entry_t)].mode == FILE)
        // {
        //     inode_t *target_file = lookup(par_inode, rm_dir[i/sizeof(dir_entry_t)].name, FILE);
        //     if(target_file != NULL)
        //     {
        //         clear_block(target_file->direct_ptr[0]);
        //         clear_inode_map(target_file->ino);
        //     }
        // }
        if(rm_dir[i/sizeof(dir_entry_t)].mode == FILE)
        {
            do_rm(rm_dir[i/sizeof(dir_entry_t)].name);
        }
        else if(rm_dir[i/sizeof(dir_entry_t)].mode == DIR)
        {
            do_rmdir(rm_dir[i/sizeof(dir_entry_t)].name);
        }
    }
    current_inode = par_inode;
    par_inode->link_count--;
    par_inode->used_size -= sizeof(dir_entry_t);
    par_inode->modefy_time = get_timer();
    write_inode(par_inode->ino);

    printk("[FileSystem] delete a directory!\n");
    return 0;
}

int do_ls(char *path, int option)
{
    bios_sd_read(SUPERBLOCK_ADDR, 1, FS_START);
    superblock_t *sb = (superblock_t *)(pa2kva(SUPERBLOCK_ADDR));
    if(sb->magic != SUPERBLOCK_MAGIC)
    {
        printk("[FileSystem] Magic Number Error!\n");
        return -1;
    }
    bios_sd_read(INODE_ADDR, 1, FS_START+INODE_OFFSET);
    if(state == 0)
    {
        current_inode = (inode_t *)(pa2kva(INODE_ADDR));
        state = 1;
    }

    inode_t *temp_inode = current_inode;
    if(path[0] != '\0')
    {
        int mark = find_dir(path, DIR);
        if(!mark)
        {
            current_inode = temp_inode;
        }
    }

    if(option == 0)
    {
        bios_sd_read(DATA_BLOCK_ADDR, 8, FS_START+(current_inode->direct_ptr[0])*8);
        dir_entry_t *dir_ptr = (dir_entry_t *)(pa2kva(DATA_BLOCK_ADDR));
        for(int i = 0; i < current_inode->used_size; i+=sizeof(dir_entry_t))
        {
            printk("%s ", dir_ptr[i/sizeof(dir_entry_t)].name);
        }
        printk("\n");
        current_inode = temp_inode;
    }
    else if(option == 1)
    {
        bios_sd_read(DATA_BLOCK_ADDR, 8, FS_START+(current_inode->direct_ptr[0])*8);
        dir_entry_t *dir_ptr = (dir_entry_t *)(pa2kva(DATA_BLOCK_ADDR));
        for(int i = 0; i < current_inode->used_size; i+=sizeof(dir_entry_t))
        {
            inode_t *file = get_inode(dir_ptr[i/sizeof(dir_entry_t)].ino);
            if(file->mode == DIR)
                printk("%s used size: %dB link nums:%d ino: %d mode:DIR\n",dir_ptr[i/sizeof(dir_entry_t)].name,file->used_size,file->link_count,file->ino);
            else 
                printk("%s used size: %dB link nums:%d ino: %d mode:FILE\n",dir_ptr[i/sizeof(dir_entry_t)].name,file->used_size,file->link_count,file->ino);
            
        }
        printk("\n");
        current_inode = temp_inode;
    }
    return 0;
}

int do_touch(char *path)
{
    bios_sd_read(SUPERBLOCK_ADDR, 1, FS_START);
    superblock_t *sb = (superblock_t *)(pa2kva(SUPERBLOCK_ADDR));
    if(sb->magic != SUPERBLOCK_MAGIC)
    {
        printk("[FileSystem] Error:Magic Number Error!\n");
        return -1;
    }

    //检查current_inode有没有值，没有就指向根目录
    bios_sd_read(INODE_ADDR, 1, FS_START+INODE_OFFSET);
    if(state == 0)
    {
        current_inode = (inode_t *)(pa2kva(INODE_ADDR));
        state = 1;
    }

    //检查文件是否已经存在
    inode_t *par_inode = current_inode;
    inode_t *target_file = lookup(par_inode, path, FILE);
    if(target_file != NULL)
    {
        printk("[FileSystem] Error:File already exists!\n");
        return -1;
    }

    //分配inode和block
    uint32_t inode_num = alloc_inode();
    inode_t *new_inode = (inode_t *)(pa2kva(INODE_ADDR)+inode_num*SECTOR_SIZE);
    new_inode->ino = inode_num;
    new_inode->mode = FILE;
    new_inode->access = RW_A;
    new_inode->link_count = 1;
    new_inode->size = 0;
    new_inode->used_size = 0;
    new_inode->start_time = get_timer();
    new_inode->modefy_time = get_timer();
    new_inode->direct_ptr[0] = alloc_data_block();
    for(int i = 1; i < MAX_DIRECT_NUM; i++)
    {
        new_inode->direct_ptr[i] = 0;
    }
    new_inode->L1_ptr[0] = 0;
    new_inode->L1_ptr[1] = 0;
    new_inode->L1_ptr[2] = 0;
    new_inode->L2_ptr[0] = 0;
    new_inode->L2_ptr[1] = 0;
    new_inode->L3_ptr = 0;
    write_inode(new_inode->ino);

    //父目录做改动
    bios_sd_read(DATA_BLOCK_ADDR, 8, FS_START + (par_inode->direct_ptr[0])*8);
    dir_entry_t *dir_entry = (dir_entry_t *)(pa2kva(DATA_BLOCK_ADDR) + par_inode->used_size);
    dir_entry->mode = FILE;
    dir_entry->ino = new_inode->ino;
    strcpy(dir_entry->name, path);
    bios_sd_write(DATA_BLOCK_ADDR, 8, FS_START + (par_inode->direct_ptr[0])*8);
    par_inode->link_count++;
    par_inode->used_size += sizeof(dir_entry_t);
    par_inode->modefy_time = get_timer();
    write_inode(par_inode->ino);

    printk("create file success\n");
    
    return 0;
}

int do_cat(char *path)
{
    bios_sd_read(SUPERBLOCK_ADDR, 1, FS_START);
    superblock_t *sb = (superblock_t *)(pa2kva(SUPERBLOCK_ADDR));
    if(sb->magic != SUPERBLOCK_MAGIC)
    {
        printk("[FileSystem] Error:Magic Number Error!\n");
        return -1;
    }

    //检查current_inode有没有值，没有就指向根目录
    bios_sd_read(INODE_ADDR, 1, FS_START+INODE_OFFSET);
    if(state == 0)
    {
        current_inode = (inode_t *)(pa2kva(INODE_ADDR));
        state = 1;
    }

    inode_t *temp_inode = current_inode;
    inode_t *target_file;
    if(find_dir(path, FILE))
    {
        target_file = current_inode;
    }
    else
    {
        printk("[FileSystem] Error:can not find this file");
        return -1;
    }
    current_inode = temp_inode;

    if(target_file == NULL)
    {
        printk("[FileSystem] Error:can not find this file");
        return -1;
    }
    if(target_file->mode != FILE)
    {
        printk("[FileSystem] Error:this is not a file");
        return -1;
    }

    int n = 0;
    for(int r_ptr = 0; r_ptr < target_file->used_size; r_ptr += BLOCK_SIZE)
    {
        for(int j = 0; j < 8; j++)
        {
            clear_sector(pa2kva(DATA_BLOCK_ADDR) + j*SECTOR_SIZE);
        }
        uint32_t block_id = get_block_addr(*target_file, r_ptr);
        bios_sd_read(DATA_BLOCK_ADDR, 8, FS_START+8*block_id);
        printk("[FileSystem] %d", r_ptr/BLOCK_SIZE);
        char *buf = (char *)pa2kva(DATA_BLOCK_ADDR);
        for(int k = 0; k < BLOCK_SIZE && n < target_file->used_size; k++, n++)
        {
            printk("%c", buf[k]);
        }

    }

    return 0;
}

int do_fopen(char *path, int mode)
{
    bios_sd_read(SUPERBLOCK_ADDR, 1, FS_START);
    superblock_t *sb = (superblock_t *)(pa2kva(SUPERBLOCK_ADDR));
    if(sb->magic != SUPERBLOCK_MAGIC)
    {
        printk("[FileSystem] Error:Magic Number Error!\n");
        return -1;
    }

    bios_sd_read(INODE_ADDR, 1, FS_START+INODE_OFFSET);
    if(state == 0)
    {
        current_inode = (inode_t *)(pa2kva(INODE_ADDR));
        state = 1;
    }

    //先找文件
    inode_t *temp_inode = current_inode;
    inode_t *target_file = NULL;
    if(find_dir(path, FILE))
    {
        target_file = current_inode;
    }
    else
    {
        printk("[FileSystem] Error:can not find this file");
        return -1;
    }
    current_inode = temp_inode;

    if(target_file->mode != FILE)
    {
        printk("[FileSystem] Error:can not find this file");
        return -1;
    }

    //检查权限
    if(target_file->access != RW_A && target_file->access != mode)
    {
        printk("[FileSystem] Error:Permission Denied!\n");
        return -1;
    }

    //分配fd
    int fd = 0;
    for(int i = 0; i < NUM_FDESCS; i++)
    {
        if(fdesc_array[i].ino == 0)
        {
            fd = 1;
            break;
        }
        if(i == NUM_FDESCS - 1)
        {
            printk("[FileSystem] Error:fd is full\n");
            return -1;
        }
    }

    fdesc_array[fd].ino = target_file->ino;
    fdesc_array[fd].mode = mode;
    fdesc_array[fd].r_ptr = 0;
    fdesc_array[fd].w_ptr = 0;

    return fd;
}

int do_fread(int fd, char *buff, int length)
{
    bios_sd_read(SUPERBLOCK_ADDR, 1, FS_START);
    superblock_t *sb = (superblock_t *)(pa2kva(SUPERBLOCK_ADDR));
    if(sb->magic != SUPERBLOCK_MAGIC)
    {
        printk("[FileSystem] Error:Magic Number Error!\n");
        return -1;
    }
    if(fd>=NUM_FDESCS || fd<0)
    {
        printk("[FileSystem] Error: invalid file descriptor!\n");
        return -1;
    }
    //判断文件可不可读
    if(fdesc_array[fd].mode != FOPEN_RDONLY && fdesc_array[fd].mode != FOPEN_RDWR)
    {
        printk("[FileSystem] Error:Permission Denied!\n");
        return -1;
    }

    bios_sd_read(INODE_ADDR, 1, FS_START+INODE_OFFSET);
    if(state == 0)
    {
        current_inode = (inode_t *)(pa2kva(INODE_ADDR));
        state = 1;
    }

    //获取文件
    inode_t *target_file = get_inode(fdesc_array[fd].ino);
    int len = (length > MAX_FILE_SIZE - fdesc_array[fd].r_ptr) ? (MAX_FILE_SIZE-fdesc_array[fd].r_ptr) : length;
    int read_ptr = fdesc_array[fd].r_ptr;
    while(read_ptr < fdesc_array[fd].r_ptr + len)
    {
        int part_len = (read_ptr % BLOCK_SIZE) ? (BLOCK_SIZE-(read_ptr%BLOCK_SIZE)) : BLOCK_SIZE;
        int tmp = fdesc_array[fd].r_ptr + len - read_ptr;
        part_len = part_len > tmp ? tmp : part_len;
        uint32_t read_block = get_block_addr(*target_file, read_ptr);
        bios_sd_read(kva2pa((uint64_t)buffer), BLOCK_SIZE/SECTOR_SIZE, FS_START + read_block*8);
        memcpy((uint8_t*)buff, (uint8_t*)(buffer+(read_ptr%BLOCK_SIZE)), part_len);
        read_ptr += part_len;
        buff += part_len;
    }
    fdesc_array[fd].r_ptr += len;

    target_file->modefy_time = get_timer();
    write_inode(target_file->ino);
    return len;
}

int do_fwrite(int fd, char *buff, int length)
{
    bios_sd_read(SUPERBLOCK_ADDR, 1, FS_START);
    superblock_t *sb = (superblock_t *)(pa2kva(SUPERBLOCK_ADDR));
    if(sb->magic != SUPERBLOCK_MAGIC)
    {
        printk("[FileSystem] Error:Magic Number Error!\n");
        return -1;
    }
    if(fd>=NUM_FDESCS || fd<0)
    {
        printk("[FileSystem] Error: invalid file descriptor!\n");
        return -1;
    }
    //判断文件可不可写
    if(fdesc_array[fd].mode != FOPEN_WRONLY && fdesc_array[fd].mode != FOPEN_RDWR)
    {
        printk("[FileSystem] Error:Permission Denied!\n");
        return -1;
    }

    bios_sd_read(INODE_ADDR, 1, FS_START+INODE_OFFSET);
    if(state == 0)
    {
        current_inode = (inode_t *)(pa2kva(INODE_ADDR));
        state = 1;
    }

    inode_t *target_file = get_inode(fdesc_array[fd].ino);
    int len = (length > MAX_FILE_SIZE-fdesc_array[fd].w_ptr) ? (MAX_FILE_SIZE-fdesc_array[fd].w_ptr) : length;
    //
    int writr_ptr = fdesc_array[fd].w_ptr;
    while(writr_ptr < fdesc_array[fd].w_ptr+len)
    {
        int part_len = writr_ptr%BLOCK_SIZE ? (BLOCK_SIZE-(writr_ptr%BLOCK_SIZE)) : BLOCK_SIZE;
        int tmp = fdesc_array[fd].w_ptr+len-writr_ptr;
        part_len = (tmp>part_len) ? part_len : tmp;
        uint32_t write_block = get_block_addr(*target_file, writr_ptr);
        if(writr_ptr % BLOCK_SIZE || part_len < BLOCK_SIZE)
        {
            bios_sd_read(kva2pa((uint64_t)buffer), 8, FS_START + write_block*8);
        }
        memcpy((uint8_t *)(buffer + writr_ptr%BLOCK_SIZE), (uint8_t *)buff, part_len);
        bios_sd_write(kva2pa((uint64_t)buffer), 8, FS_START + write_block*8);
        writr_ptr += part_len;
        buff += part_len;
    }
    fdesc_array[fd].w_ptr += len;

    target_file->used_size = fdesc_array[fd].w_ptr;
    target_file->modefy_time = get_timer();
    if(fdesc_array[fd].w_ptr > target_file->size)
    {
        target_file->size = fdesc_array[fd].w_ptr;
    }
    write_inode(target_file->ino);

    return len;
}

int do_fclose(int fd)
{
    fdesc_array[fd].ino = 0;
    fdesc_array[fd].mode = 0;
    fdesc_array[fd].r_ptr = 0;
    fdesc_array[fd].w_ptr = 0;

    return 0;
}

int do_ln(char *src_path, char *dst_path)
{
    bios_sd_read(SUPERBLOCK_ADDR, 1, FS_START);
    superblock_t *sb = (superblock_t *)(pa2kva(SUPERBLOCK_ADDR));
    if(sb->magic != SUPERBLOCK_MAGIC)
    {
        printk("[FileSystem] Error:Magic Number Error!\n");
        return -1;
    }

    //检查current_inode有没有值，没有就指向根目录
    bios_sd_read(INODE_ADDR, 1, FS_START+INODE_OFFSET);
    if(state == 0)
    {
        current_inode = (inode_t *)(pa2kva(INODE_ADDR));
        state = 1;
    }

    inode_t *par_inode = current_inode;
    inode_t *target_file;
    if(find_dir(src_path, FILE))
    {
        target_file = current_inode;
    }
    else
    {
        printk("[FileSystem] Error:can not find this file");
        return -1;
    }
    current_inode = par_inode;

    //建立硬链接
    bios_sd_read(DATA_BLOCK_ADDR, 8, FS_START+(par_inode->direct_ptr[0])*8);
    dir_entry_t *dir_entry = (dir_entry_t *)(pa2kva(DATA_BLOCK_ADDR) + par_inode->used_size);
    dir_entry->mode = FILE;
    dir_entry->ino = target_file->ino;
    strcpy(dir_entry->name, dst_path);
    bios_sd_write(DATA_BLOCK_ADDR, 8, FS_START+(par_inode->direct_ptr[0])*8);
    //调整par_inode
    par_inode->link_count++;
    par_inode->used_size += sizeof(dir_entry_t);
    par_inode->modefy_time = get_timer();
    write_inode(par_inode->ino);

    target_file->link_count++;
    write_inode(target_file->ino);
    printk("[FileSystem] Success:create hard link");

    return 0;
}

int do_rm(char *path)
{
    bios_sd_read(SUPERBLOCK_ADDR, 1, FS_START);
    superblock_t *sb = (superblock_t *)(pa2kva(SUPERBLOCK_ADDR));
    if(sb->magic != SUPERBLOCK_MAGIC)
    {
        printk("[FileSystem] Error:Magic Number Error!\n");
        return -1;
    }

    //检查current_inode有没有值，没有就指向根目录
    bios_sd_read(INODE_ADDR, 1, FS_START+INODE_OFFSET);
    if(state == 0)
    {
        current_inode = (inode_t *)(pa2kva(INODE_ADDR));
        state = 1;
    }

    inode_t *par_inode = current_inode;
    inode_t *target_file = lookup(par_inode, path, FILE);

    if(target_file == NULL)
    {
        printk("[FileSystem] Error:No such file or directory!\n");
        return -1;
    }
    if(target_file->mode != FILE)
    {
        printk("[FileSystem] Error:Not a file!\n");
        return -1;
    }
    int found = 0;
    bios_sd_read(DATA_BLOCK_ADDR, 8, FS_START+(par_inode->direct_ptr[0])*8);
    dir_entry_t *dir_entry = (dir_entry_t *)(pa2kva(DATA_BLOCK_ADDR));
    for(int i = 0; i < par_inode->used_size; i+=sizeof(dir_entry_t))
    {
        if(found)
        {
            memcpy((uint8_t*)(dir_entry-1), (uint8_t*)dir_entry, sizeof(dir_entry_t));
        }
        if(!strcmp(path, dir_entry->name))
        {
            found = 1;
        }
        dir_entry++;
    }
    bios_sd_write(DATA_BLOCK_ADDR, 8, FS_START+(par_inode->direct_ptr[0])*8);
    //inode
    par_inode->used_size -= sizeof(dir_entry_t);
    par_inode->link_count--;
    par_inode->modefy_time = get_timer();
    write_inode(par_inode->ino);

    target_file->link_count--;
    if(target_file->link_count == 0)
    {
        clear_inode_map(target_file->ino);

        for(int i = 0; i < MAX_DIRECT_NUM; i++)
        {
            if(target_file->direct_ptr[i] != 0)
            {
                clear_block(target_file->direct_ptr[i]);
            }
        }

        for(int i = 0; i < 3; i++)
        {
            if(target_file->L1_ptr[i] != 0)
            {
                recycle_level(target_file->L1_ptr[i], 1);
            }
        }
        for(int i = 0; i < 2; i++)
        {
            if(target_file->L2_ptr[i] != 0)
            {
                recycle_level(target_file->L2_ptr[i], 2);
            }
        }
        if(target_file->L3_ptr != 0)
        {
            recycle_level(target_file->L3_ptr, 3);
        }
    }
    else 
    {
        write_inode(target_file->ino);
    }
    printk("remove file success\n");
    return 0;
}

int do_lseek(int fd, int offset, int whence)
{
    bios_sd_read(SUPERBLOCK_ADDR, 1, FS_START);
    superblock_t *sb = (superblock_t *)(pa2kva(SUPERBLOCK_ADDR));
    if(sb->magic != SUPERBLOCK_MAGIC)
    {
        printk("[FileSystem] Error:Magic Number Error!\n");
        return -1;
    }

    //检查current_inode有没有值，没有就指向根目录
    bios_sd_read(INODE_ADDR, 1, FS_START+INODE_OFFSET);
    if(state == 0)
    {
        current_inode = (inode_t *)(pa2kva(INODE_ADDR));
        state = 1;
    }

    inode_t *l_file = get_inode(fdesc_array[fd].ino);
    if(whence == SEEK_SET)
    {
        fdesc_array[fd].r_ptr = offset;
        fdesc_array[fd].w_ptr = offset;
    }
    else if(whence == SEEK_CUR)
    {
        fdesc_array[fd].r_ptr += offset;
        fdesc_array[fd].w_ptr += offset;
    }
    else if(whence == SEEK_END)
    {
        fdesc_array[fd].r_ptr = l_file->size + offset;
        fdesc_array[fd].w_ptr = l_file->size + offset;
    }

    return fdesc_array[fd].r_ptr;
}