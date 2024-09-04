#ifndef __UNISTD_H__
#define __UNISTD_H__

#include <stdint.h>

void sys_sleep(uint32_t time);
void sys_yield(void);
void sys_write(char *buff);
void sys_move_cursor(int x, int y);
void sys_reflush(void);
long sys_get_timebase(void);
long sys_get_tick(void);
int sys_mutex_init(int key);
void sys_mutex_acquire(int mutex_idx);
void sys_mutex_release(int mutex_idx);
void thread_create(void(*thread_count)(void*), int *res_t1, int *res_t2, int *yield_times);
void thread_yield();

/************************************************************/
/* Do not touch this comment. Reserved for future projects. */
/************************************************************/

#endif
