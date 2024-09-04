#ifndef __INCLUDE_TASK_H__
#define __INCLUDE_TASK_H__

#include <type.h>

#define TASK_MEM_BASE    0x52000000
#define TASK_MAXNUM      32
#define TASK_SIZE        0x10000


#define SECTOR_SIZE 512
#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))
#define MAXNAME 32

/* TODO: [p1-task4] implement your own task_info_t! */
typedef struct {
    char task_name[MAXNAME];
    int task_size;
    int task_offset;
    int task_memsz;
    uint64_t task_entry;
} task_info_t;

extern task_info_t tasks[TASK_MAXNUM];

#endif