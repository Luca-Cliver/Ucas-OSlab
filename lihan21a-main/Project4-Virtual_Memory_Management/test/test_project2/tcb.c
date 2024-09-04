#include <stdio.h>
#include <unistd.h>  // NOTE: use this header after implementing syscall!
#include <kernel.h>

int flag = 0;

void thread_1(int *res_t1, int *res_t2, int *yield_times)
{
   (*res_t1) = 0;
   (*res_t2) = 0;

    for (int i = 0; ; i++)
    {
        while(*res_t1 - *res_t2 >= 4)
        {
          //  (*yield_times)++;
          // flag++;
            __sync_fetch_and_add(&flag, 1);
            thread_yield();
            //sys_yield();
        }
        //(*res_t1)++;
        __sync_fetch_and_add(res_t1, 1);
        sys_move_cursor(0,6);
        printf("> [thread1] This is in the 1st thread! (%d)\n", *res_t1);

    }
}

 void thread_2(int *res_t1, int *res_t2, int *yield_times)
 {
    for (int i = 0; ; i++)
    {
        while(*res_t2 - *res_t1 >= 4)
        {
           // (*yield_times)++;
           //flag++;
            __sync_fetch_and_add(&flag, 1);
            thread_yield();
          //sys_yield();
        }
        //(*res_t2)++;
        __sync_fetch_and_add(res_t2, 1);
        sys_move_cursor(0,7);
        printf("> [thread2] This is in the 2nd thread! (%d)\n", *res_t2);
    }
 }

int main()
{
    int32_t pthread_1 = 1, pthread_2 = 2;
    int res_t1 = 0, res_t2 = 0, res_sum = 0;
    int yield_times = 0;
    flag = 0;

    thread_create(thread_1, &res_t1, &res_t2, &yield_times);
    thread_create(thread_2, &res_t1, &res_t2, &yield_times);

    
    while(1)
    {
        sys_move_cursor(0,9);
        printf("> [tcb_sch] yield times = %d\n", flag);
    }
}