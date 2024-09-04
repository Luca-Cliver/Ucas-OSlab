#include "type.h"
#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <os/time.h>
#include <os/kernel.h>

pcb_t pcb[NUM_MAX_TASK];
pcb_t tcb[NUM_MAX_TASK];
int thread_idx = 0;
int thread1_yield_times = 0;
int thread2_yield_times = 0;
int thread_yield_times = 0;

extern void ret_from_exception();

const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);
LIST_HEAD(thread_queue);

/* current running task PCB */
pcb_t * volatile current_running;
pcb_t * volatile pre_running = &pid0_pcb;

/* global process id */
pid_t process_id = 1;

void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs

    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/

    // TODO: [p2-task1] Modify the current_running pointer.
    bios_set_timer(get_ticks() + 150000);
    pre_running = current_running;
    check_sleeping();
    if(pre_running->status == TASK_RUNNING)
    {
        list_add(&pre_running->list, &ready_queue);
        pre_running->status = TASK_READY;
    }
    
    if(!list_is_empty(&ready_queue))
    {
        current_running = (pcb_t *)((uint64_t)(ready_queue.next) - 2*sizeof(reg_t));
        list_del(&(current_running->list));
        current_running->status = TASK_RUNNING;
    }
    else 
    {
        
        current_running = &pid0_pcb;
        current_running->status = TASK_RUNNING;
    }
    if(current_running == &tcb[0] || current_running == &tcb[1])
    {
        thread_yield_times++;
        screen_move_cursor(0, 8);
        if(current_running == &tcb[0])
            thread1_yield_times++;
        else
            thread2_yield_times++;
        printk("thread1: %d thread2: %d thread: %d", thread1_yield_times, thread2_yield_times, thread_yield_times);
        //printk("thread yield time is %d\n", thread_yield_times);
    }
    // TODO: [p2-task1] switch_to current_running
    switch_to(pre_running, current_running);
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.
    current_running ->status = TASK_BLOCKED;
    current_running ->wakeup_time = sleep_time + get_timer();
    list_add(&current_running->list, &sleep_queue);
    do_scheduler();
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    list_add(pcb_node, queue);
    ((pcb_t*)((uint64_t)(pcb_node) - 2*sizeof(reg_t)))->status = TASK_BLOCKED;
    do_scheduler();
}

void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    list_del(pcb_node);
    ((pcb_t*)((uint64_t)(pcb_node) - 2*sizeof(reg_t)))->status = TASK_READY;
    list_add(pcb_node, &ready_queue);
}

static void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb)
{
     /* TODO: [p2-task3] initialization of registers on kernel stack
      * HINT: sp, ra, sepc, sstatus
      * NOTE: To run the task in user mode, you should set corresponding bits
      *     of sstatus(SPP, SPIE, etc.).
      */
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    pt_regs->regs[1] = entry_point;  //ra
    pt_regs->regs[2] = user_stack;   //sp
    pt_regs->regs[4] = pcb;

    pt_regs->sepc = entry_point;
    pt_regs->sstatus = (1 << 1);
    //pt_regs->sstatus = (reg_t) (SR_SIE);

    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
//    pcb->kernel_sp = (reg_t)pt_switchto;
    pcb->kernel_sp = kernel_stack - sizeof(switchto_context_t) - sizeof(regs_context_t);
    pcb->user_sp = user_stack;
    pcb -> entry_point = entry_point;

    //pt_switchto->regs[0] = entry_point; //ra
    pt_switchto->regs[0] = &ret_from_exception; //ra
    pt_switchto->regs[1] = pcb->kernel_sp;  // sp

}

/*void thread_yield()
{
    
    pcb_t *thread_tobe_ready = thread_queue.next;
    list_del(thread_queue.next);
    list_add(thread_tobe_ready, &ready_queue);
    
    
    
    
    pre_running = current_running;
    pre_running->status = TASK_READY;
    current_running =  (pcb_t *)((uint64_t)(ready_queue.next) - 2*sizeof(reg_t));
    current_running->status = TASK_RUNNING;
    list_del(&(current_running->list));
    list_add(&pre_running->list, &thread_queue);
    switch_to(pre_running, current_running);
    
}*/

void thread_yield()
{
   // bios_set_timer(get_ticks() + 150000);
    pre_running = current_running;
    if(pre_running->thread_idx == 0)
    {
        current_running = &tcb[1];
        thread2_yield_times++;
    }
    else 
    {
        current_running = &tcb[0];
        thread1_yield_times++;
    }
    pre_running->status = TASK_READY;
    current_running->status = TASK_RUNNING;
    list_del(&(current_running->list));
    list_add(&pre_running->list, &ready_queue);

    thread_yield_times++;
    screen_move_cursor(0, 8);
    printk("thread1: %d thread2: %d thread: %d", thread1_yield_times, thread2_yield_times, thread_yield_times);
    //printk("thread yield time is %d\n", thread_yield_times);
    switch_to(pre_running, current_running);
}

void thread_create(void(*thread_count)(void*), int *res_t1, int *res_t2, int *yield_times)
{
    current_running->tcb_num++;
    pcb_t *new_tcb = &tcb[thread_idx];
    new_tcb->status = TASK_READY;
    new_tcb->tid = current_running->tcb_num;
    new_tcb->thread_idx = thread_idx++;

    uint64_t kernel_stack = allocKernelPage(1) + PAGE_SIZE;
    uint64_t user_stack   = allocUserPage(1) + PAGE_SIZE;
    uint64_t entry_point  = (uint64_t)(*thread_count);
    init_pcb_stack(kernel_stack, user_stack, entry_point, new_tcb);
    regs_context_t *pt_regs =
        (regs_context_t *)((ptr_t)(kernel_stack) - sizeof(regs_context_t));
        
    pt_regs->regs[10] = res_t1;
    pt_regs->regs[11] = res_t2;
    pt_regs->regs[12] = yield_times;

    /*if(thread_idx == 1)
        list_add(&new_tcb->list, &ready_queue);
    else
        list_add(&new_tcb->list, &thread_queue);*/
    list_add(&new_tcb->list, &ready_queue);
}



