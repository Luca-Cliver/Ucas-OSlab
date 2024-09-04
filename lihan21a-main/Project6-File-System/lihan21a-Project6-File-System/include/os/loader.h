#ifndef __INCLUDE_LOADER_H__
#define __INCLUDE_LOADER_H__

#include <type.h>

#define USER_TEXT_BASE 0x10000

uint64_t load_task_img(int taskid, uintptr_t pgdir);

#endif