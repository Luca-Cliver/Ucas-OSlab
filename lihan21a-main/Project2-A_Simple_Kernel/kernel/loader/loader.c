#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>

#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

uint64_t load_task_img(int taskid)
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
    int block_num = NBYTES2SEC(tasks[taskid].task_offset % SECTOR_SIZE + tasks[taskid].task_size);
    int block_id = tasks[taskid].task_offset / SECTOR_SIZE;
    uint64_t enter_point = tasks[taskid].task_entry;
    bios_sd_read(enter_point, block_num, block_id);
    memcpy((uint8_t *)enter_point, (uint8_t *)(enter_point + tasks[taskid].task_offset%SECTOR_SIZE), tasks[taskid].task_size);
    return enter_point;
    
}