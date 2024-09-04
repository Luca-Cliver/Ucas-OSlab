#include "os/mm.h"
#include "pgtable.h"
#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <os/loader.h>
#include <type.h>

#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

uint64_t load_task_img(int taskid, uint64_t pgdir)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */
     /*task3*/
/*   uint64_t enter_point = TASK_MEM_BASE + TASK_SIZE*taskid;
     uint64_t block_id = (taskid + 1)*15 + 1;         //kernal有15个，bootblock有一个
     bios_sd_read(enter_point, 15, block_id);
     return enter_point;
*/
    //int fileblock_num = NBYTES2SEC(tasks[taskid].task_offset % SECTOR_SIZE + tasks[taskid].task_size);
    //int block_num = NBYTES2SEC(tasks[taskid].task_offset % SECTOR_SIZE + tasks[taskid].task_memsz);
    
    int needed_page_num = (tasks[taskid].task_memsz - 1 + PAGE_SIZE) / PAGE_SIZE;
    //int filepagenum = (fileblock_num / 8) + 1;
    uint64_t enter_point = tasks[taskid].task_entry;
    int block_id = tasks[taskid].task_offset / SECTOR_SIZE;
    static char temp_sdread[2*PAGE_SIZE];
    uint64_t kva_context = 0;
    for(int i = 0; i < needed_page_num; i++)
    {
        kva_context = alloc_page_helper(enter_point + i*PAGE_SIZE, pgdir);
        bios_sd_read(kva2pa((uintptr_t)temp_sdread), 16, block_id);
        memcpy(
            (uint8_t *)(kva_context),
            (uint8_t *)(temp_sdread + tasks[taskid].task_offset%SECTOR_SIZE),
            PAGE_SIZE
        );
        block_id += 8;
    }    
     /*
    for(int i = 0; i < pagenum; i++)
    {
        if(i < filepagenum)
        {
            uintptr_t kva_context = alloc_page_helper(enter_point + i*PAGE_SIZE, pgdir);
            bios_sd_read(kva2pa((uintptr_t)temp_sdread), 16, block_id);
            
            memcpy(
            (void*)kva_context, 
            (void*)(temp_sdread + tasks[taskid].task_offset%SECTOR_SIZE), 
            PAGE_SIZE
            );
        }
        else //bss 
        {
            uintptr_t kva_bss = alloc_page_helper(enter_point + i*PAGE_SIZE, pgdir);
            for(int j = 0; j < PAGE_SIZE; j++)
            {
                *((uint64_t*)kva_bss + j) = 0;
            }
        }
        block_id += 8;
    }*/
    //bios_sd_read(enter_point, block_num, block_id);
    //memcpy((uint8_t *)enter_point, (uint8_t *)(enter_point + tasks[taskid].task_offset%SECTOR_SIZE), tasks[taskid].task_memsz);
    return enter_point;
    
}