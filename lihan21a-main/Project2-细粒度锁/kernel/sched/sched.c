#include "os/smp.h"
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
#include <os/task.h>
#include <os/string.h>

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
    .mask = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack
};

const ptr_t pid0_stack_promax = INIT_KERNEL_STACK_SLAVE + PAGE_SIZE;
pcb_t pid0_pcb_promax = {
    .pid = 0,
    .mask = 0,
    .kernel_sp = (ptr_t)pid0_stack_promax,
    .user_sp = (ptr_t)pid0_stack_promax
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);
LIST_HEAD(thread_queue);

/* current running task PCB */
extern pcb_t * volatile current_running[2];
extern pcb_t * volatile pre_running;

/* global process id */
pid_t process_id = 1;

spin_lock_t sched_lock;
spin_lock_t ready_queue_lock;
spin_lock_t current_running_lock[2];
spin_lock_t pre_running_lock;

void little_lock_init()
{
    spin_lock_init(&sched_lock);
    spin_lock_init(&ready_queue_lock);
    spin_lock_init(&current_running_lock[0]);
    spin_lock_init(&current_running_lock[1]);
    spin_lock_init(&pre_running_lock);
}
void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs

    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/

    // TODO: [p2-task1] Modify the current_running[cpu_id] pointer.
    //bios_set_timer(get_ticks() + 150000);
    spin_lock_acquire(&sched_lock);
    uint64_t cpu_id = get_current_cpu_id();
    pre_running = current_running[cpu_id];
    spin_lock_acquire(&pre_running_lock);
    spin_lock_acquire(&pre_running->spinlock);
    check_sleeping();
    

    if(!pre_running->pid)
    {
        pre_running->status = TASK_BLOCKED;
    }

    if(pre_running->status == TASK_RUNNING || 
       pre_running->status == TASK_READY)
    {
        spin_lock_acquire(&ready_queue_lock);
        list_add(&pre_running->list, &ready_queue);
        spin_lock_release(&ready_queue_lock);
        pre_running->status = TASK_READY;
    }
    //pre_running->core_id = -1;

    list_node_t *temp_list;
    pcb_t *temp_pcb;
    //pcb_t *node = current_running[1-cpu_id];

    spin_lock_acquire(&ready_queue_lock);
    spin_lock_acquire(&current_running_lock[cpu_id]);
    if(!list_is_empty(&ready_queue))
    {
        temp_list = ready_queue.next;
        temp_pcb = LIST2PCB(temp_list);
        while((temp_pcb->mask != 0)  &&
             !((temp_pcb->mask == 1) && (cpu_id == 0)) &&
             !((temp_pcb->mask == 2) && (cpu_id == 1)) && 
              (temp_list != &ready_queue))
        {
            temp_list = temp_list->next;
            temp_pcb = LIST2PCB(temp_list);
        }
        if(temp_list == &ready_queue)
        {
            current_running[cpu_id] = (cpu_id == 0) ? &pid0_pcb : newcore_pcb;
        }
        else
        {
            current_running[cpu_id] = temp_pcb;
            list_del(&(current_running[cpu_id]->list));
            //current_running[1-cpu_id] = node;
            //current_running[cpu_id]->status = TASK_RUNNING;
        }        
    }
    else if(cpu_id == 0)
    {
        current_running[cpu_id] = &pid0_pcb;
    }
    else if(cpu_id == 1)
    {
        current_running[cpu_id] = newcore_pcb;
        //current_running[cpu_id] = &pid0_pcb_promax;
    }

    if(current_running[cpu_id] != pre_running)
        spin_lock_acquire(&current_running[cpu_id]->spinlock);

    current_running[cpu_id]->core_id = cpu_id;
    current_running[cpu_id]->status = TASK_RUNNING;
    if(current_running[cpu_id] == &tcb[0] || current_running[cpu_id] == &tcb[1])
    {
        thread_yield_times++;
        screen_move_cursor(0, 8);
        if(current_running[cpu_id] == &tcb[0])
            thread1_yield_times++;
        else
            thread2_yield_times++;
        printk("thread1: %d thread2: %d thread: %d", thread1_yield_times, thread2_yield_times, thread_yield_times);
        //printk("thread yield time is %d\n", thread_yield_times);
    }
    // TODO: [p2-task1] switch_to current_running[cpu_id]
    spin_lock_release(&ready_queue_lock);
    switch_to(pre_running, current_running[cpu_id]);
    //screen_reflush();
}

void sched_release(void)
{
    uint64_t cpu_id = get_current_cpu_id();
    spin_lock_release(&current_running[cpu_id]->spinlock);
    spin_lock_release(&current_running_lock[cpu_id]);
    spin_lock_release(&pre_running->spinlock);
    spin_lock_release(&pre_running_lock);
    spin_lock_release(&sched_lock);
}


void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running[cpu_id]
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running[cpu_id] is blocked.
    uint64_t cpu_id = get_current_cpu_id();
    spin_lock_acquire(&current_running[cpu_id]->spinlock);
    spin_lock_acquire(&current_running_lock[cpu_id]);
    current_running[cpu_id] ->status = TASK_BLOCKED;
    current_running[cpu_id] ->wakeup_time = sleep_time + get_timer();
    list_add(&current_running[cpu_id]->list, &sleep_queue);
    spin_lock_release(&current_running_lock[cpu_id]);
    spin_lock_release(&current_running[cpu_id]->spinlock);
    do_scheduler();
}

void do_block(list_node_t *pcb_node, list_head *queue, spin_lock_t *block_lock)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    pcb_t *temp_pcb = LIST2PCB(pcb_node);
    spin_lock_acquire(&(temp_pcb->spinlock));
    list_add(pcb_node, queue);
    temp_pcb->status = TASK_BLOCKED;
    spin_lock_release(block_lock);
    spin_lock_release(&(temp_pcb->spinlock));
    //((pcb_t*)((uint64_t)(pcb_node) - 2*sizeof(reg_t)))->status = TASK_BLOCKED;
    //LIST2PCB(pcb_node)->status = TASK_BLOCKED;
    do_scheduler();
    spin_lock_acquire(block_lock);
}

void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    pcb_t *temp_pcb = LIST2PCB(pcb_node);
    spin_lock_acquire(&(temp_pcb->spinlock));
    list_del(pcb_node);
    //((pcb_t*)((uint64_t)(pcb_node) - 2*sizeof(reg_t)))->status = TASK_READY;
    temp_pcb->status = TASK_READY;
    spin_lock_acquire(&ready_queue_lock);
    //printk("unblock !!!!!!\n");
    list_add(pcb_node, &ready_queue);
    spin_lock_release(&ready_queue_lock);
    spin_lock_release(&(temp_pcb->spinlock));
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
    pt_regs->regs[1] = (reg_t)entry_point;  //ra
    pt_regs->regs[2] = (reg_t)user_stack;   //sp
    pt_regs->regs[4] = (reg_t)pcb;

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
    pt_switchto->regs[0] = (reg_t)ret_from_exception; //ra
    pt_switchto->regs[1] = pcb->kernel_sp;  // sp

}

/*void thread_yield()
{
    
    pcb_t *thread_tobe_ready = thread_queue.next;
    list_del(thread_queue.next);
    list_add(thread_tobe_ready, &ready_queue);
    
    
    
    
    pre_running = current_running[cpu_id];
    pre_running->status = TASK_READY;
    current_running[cpu_id] =  (pcb_t *)((uint64_t)(ready_queue.next) - 2*sizeof(reg_t));
    current_running[cpu_id]->status = TASK_RUNNING;
    list_del(&(current_running[cpu_id]->list));
    list_add(&pre_running->list, &thread_queue);
    switch_to(pre_running, current_running[cpu_id]);
    
}*/

void thread_yield()
{
   // bios_set_timer(get_ticks() + 150000);
    uint64_t cpu_id = get_current_cpu_id();
    pre_running = current_running[cpu_id];
    if(pre_running->thread_idx == 0)
    {
        current_running[cpu_id] = &tcb[1];
        thread2_yield_times++;
    }
    else 
    {
        current_running[cpu_id] = &tcb[0];
        thread1_yield_times++;
    }
    pre_running->status = TASK_READY;
    current_running[cpu_id]->status = TASK_RUNNING;
    list_del(&(current_running[cpu_id]->list));
    list_add(&pre_running->list, &ready_queue);

    thread_yield_times++;
    screen_move_cursor(0, 8);
    printk("thread1: %d thread2: %d thread: %d", thread1_yield_times, thread2_yield_times, thread_yield_times);
    //printk("thread yield time is %d\n", thread_yield_times);
    switch_to(pre_running, current_running[cpu_id]);
}

void thread_create(void(*thread_count)(void*), int *res_t1, int *res_t2, int *yield_times)
{
    uint64_t cpu_id = get_current_cpu_id();
    current_running[cpu_id]->tcb_num++;
    pcb_t *new_tcb = &tcb[thread_idx];
    new_tcb->status = TASK_READY;
    new_tcb->tid = current_running[cpu_id]->tcb_num;
    new_tcb->thread_idx = thread_idx++;

    uint64_t kernel_stack = allocKernelPage(1) + PAGE_SIZE;
    uint64_t user_stack   = allocUserPage(1) + PAGE_SIZE;
    uint64_t entry_point  = (uint64_t)(*thread_count);
    init_pcb_stack(kernel_stack, user_stack, entry_point, new_tcb);
    regs_context_t *pt_regs =
        (regs_context_t *)((ptr_t)(kernel_stack) - sizeof(regs_context_t));
        
    pt_regs->regs[10] = (reg_t)res_t1;
    pt_regs->regs[11] = (reg_t)res_t2;
    pt_regs->regs[12] = (reg_t)yield_times;

    /*if(thread_idx == 1)
        list_add(&new_tcb->list, &ready_queue);
    else
        list_add(&new_tcb->list, &thread_queue);*/
    list_add(&new_tcb->list, &ready_queue);
}

int do_kill(pid_t pid)
{
    pcb_t *target_pcb;

    int i = 0;
    for(i = 0; i < process_id; i++)
    {
        spin_lock_acquire(&pcb[i].spinlock);
        if(pcb[i].pid == pid)
        {
            if(pid == 0 || pid == 1)
            {
                spin_lock_release(&pcb[i].spinlock);
                return -1;
            }
            else 
            {
                target_pcb = &pcb[i];
                //从队列中删除
                if(target_pcb->status == TASK_READY || target_pcb->status == TASK_BLOCKED)
                    list_del(&target_pcb->list);

                //释放锁    
                for(int j = 0; j < target_pcb->lock_num; j++)
                {
                    if(!list_is_empty(&target_pcb->lock[j]->block_queue))
                    {
                        pcb_t *pcb_to_acquire_lock
                        // = (pcb_t *)((uint64_t)((&(target_pcb->lock[i]->block_queue))->next) - 4*sizeof(reg_t));
                        = LIST2PCB((&(target_pcb->lock[j]->block_queue))->next);
                        list_del((&(target_pcb->lock[j]->block_queue))->next);
                        list_add(&(pcb_to_acquire_lock->list), &ready_queue);
                    }
                    else
                    {
                        target_pcb->lock[j]->lock.status = UNLOCKED;
                    }
                    target_pcb->lock[j] = NULL;
                }
                //清空等待队列
                while(!list_is_empty(&(target_pcb->wait_list)))
                {
                    pcb_t *pcb_to_delete
                        = LIST2PCB((target_pcb->wait_list).next);
                    if(pcb_to_delete->status == TASK_BLOCKED)
                    {
                        pcb_to_delete->status = TASK_READY;
                        list_del(&(pcb_to_delete->list));
                        list_add(&(pcb_to_delete->list), &ready_queue);
                    }
                }

                target_pcb->status = TASK_EXITED;
                target_pcb->pid = 0;
                spin_lock_release(&(pcb[i].spinlock));
                do_scheduler();
                return 1;
            }
        }
        spin_lock_release(&(pcb[i].spinlock));
    }
    return 0;
}

void do_exit(void){
    uint64_t cpu_id = get_current_cpu_id();
    //spin_lock_acquire(&(current_running_lock[cpu_id]));
    do_kill(current_running[cpu_id]->pid);
    //spin_lock_release(&(current_running_lock[cpu_id]));
}

int do_waitpid(pid_t pid)
{
    pcb_t *target_pcb;
    uint64_t cpu_id = get_current_cpu_id();
    for(int i = 0; i < process_id; i++)
    {
        spin_lock_acquire(&(pcb[i].spinlock));
        if(pcb[i].pid == pid)
        {
            if(pid == 0 || pid == 1)
            {
                spin_lock_release(&(pcb[i].spinlock));
                return 0;
            }
            else
            {           
                target_pcb = &pcb[i];
                if(pcb[i].status != TASK_EXITED)
                {
                    spin_lock_acquire(&current_running_lock[cpu_id]);
                    current_running[cpu_id]->status = TASK_BLOCKED;
                    list_add(&(current_running[cpu_id]->list), &(target_pcb->wait_list));
                    spin_lock_release(&current_running_lock[cpu_id]);
                    spin_lock_release(&(pcb[i].spinlock));
                    do_scheduler();
                    return 1;
                }
            }
        }
        spin_lock_release(&(pcb[i].spinlock));
    }
    return 0;
}

pid_t do_exec(char *name, int argc, char *argv[])
{
    process_id++;
    int succ_pid = 0;

    int i = 0;
    for(i = 0; i < NUM_MAX_TASK; i++)
    {
        if(!strcmp(name, tasks[i].task_name))
        {
            uint64_t entry_point = tasks[i].task_entry;
            for(int j = 0; j < NUM_MAX_TASK; j++)
            {
                spin_lock_acquire(&(pcb[j].spinlock));
                if(pcb[j].pid == 0)
                {
                    list_init(&(pcb[j].wait_list));
                    pcb[j].pid = process_id;
                    pcb[j].status = TASK_READY;
                    pcb[j].name = tasks[i].task_name;
                    pcb[j].mask = current_running[get_current_cpu_id()]->mask;
                    uint64_t kernel_stack = allocKernelPage(1) + PAGE_SIZE;
                    uint64_t user_stack = allocUserPage(1) + PAGE_SIZE;
                    uint64_t argv_base = user_stack - argc * sizeof(ptr_t);
                    list_add(&(pcb[j].list), &(ready_queue));
                   
                    ptr_t *p1, *p2;
                    p1 = (ptr_t *)argv_base;
                    p2 = p1;
                    for(int k = 0; k < argc; k++)
                    {
                        p2 = (ptr_t *)((uint64_t)p2 - 16);
                        p2 = (ptr_t *)strcpy(p2, argv[k]);
                        *p1 = (uint64_t)p2;
                        p1 = (ptr_t*)((uint64_t)p1 + sizeof(uint64_t));
                    }
 
                    //128bit对齐
                    int extra = (user_stack - (uint64_t)p2) % 128;
                    if(extra != 0)
                    {
                        p2 = (ptr_t *)((uint64_t)p2 - (128 - extra));
                    }
                    user_stack = (uint64_t)p2;

                    //初始化内核栈
                    regs_context_t *pt_regs =
                        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));

                    for (int n = 0; n < 32; n++){
                        pt_regs->regs[n] = 0;
                    }
                    pt_regs->regs[1] = (reg_t)entry_point; //ra
                    pt_regs->regs[2] = (reg_t)user_stack;  //sp
                    pt_regs->regs[4] = (reg_t)(&pcb[j]);
                    pt_regs->regs[10] = (reg_t)argc;
                    pt_regs->regs[11] = (reg_t)argv_base;
                    pt_regs->sepc = (reg_t)entry_point;    //sepc
                    pt_regs->sstatus = (1 << 1);    //sstatus

                    switchto_context_t *pt_switchto =
                        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

                    pt_switchto->regs[0] = (reg_t)ret_from_exception;//ra
                    pt_switchto->regs[1] = user_stack;//sp  
                    pcb[j].kernel_sp = kernel_stack - sizeof(regs_context_t) - sizeof(switchto_context_t);
                    pcb[j].user_sp   = user_stack;
                    pcb[j].entry_point = entry_point;

                    succ_pid = pcb[j].pid;
                    
                    spin_lock_release(&pcb[j].spinlock);
                    return succ_pid;
                }
                spin_lock_release(&pcb[j].spinlock);
            }
        }
    }
    return 0;
}

pid_t do_getpid()
{
    return current_running[get_current_cpu_id()]->pid;
}

void do_process_show()
{
    int live_pcb_num = 0;
    printk("[PROCESS TABLE]\n");
    printk("IDX PID STATUS  MASK TASK_NAME \n");
    for(int i = 0; i < process_id; i++)
    {
        int mask_num = pcb[i].mask == 0 ? 3 :
                       pcb[i].mask == 1 ? 1 : 2;

        if(pcb[i].status == TASK_RUNNING && pcb[i].pid != 0)
        {
            printk("[%d]  %d  RUNNING 0x%d   %s running on core %d\n",
                   live_pcb_num, pcb[i].pid, mask_num, pcb[i].name, pcb[i].core_id);
            live_pcb_num++;
        }
        else if(pcb[i].status == TASK_READY && pcb[i].pid != 0)
        {
            printk("[%d]  %d  READY   0x%d   %s ready on core %d\n",
                   live_pcb_num, pcb[i].pid,  mask_num, pcb[i].name, pcb[i].core_id);
            live_pcb_num++;
        }
        else if(pcb[i].status == TASK_BLOCKED && pcb[i].pid != 0)
        {
            printk("[%d]  %d  BLOCKED 0x%d   %s blocked on core %d\n",
                   live_pcb_num, pcb[i].pid, mask_num, pcb[i].name, pcb[i].core_id);
            live_pcb_num++;
        }
    }
}

pid_t do_taskset(char *name, int argc, char *argv[], int mask)
{
    int pid_to_taskset = do_exec(name, argc, argv);
    if(pid_to_taskset != 0)
    {
        for(int i = 0; i < NUM_MAX_TASK; i++)
        {
            spin_lock_acquire(&pcb[i].spinlock);
            if(pcb[i].pid == pid_to_taskset)
            {
                pcb[i].mask = mask;
                printk("[TASKSET] Task %d set to mask %d\n", pid_to_taskset, mask);
                spin_lock_release(&pcb[i].spinlock);
                break;
            }
            spin_lock_release(&pcb[i].spinlock);
        }
    }
    return pid_to_taskset;
}

void do_taskset_bypid(pid_t pid, int mask)
{
    int i = 0;
    for(i = 0; i < NUM_MAX_TASK; i++)
    {
        spin_lock_acquire(&pcb[i].spinlock);
        if(pcb[i].pid == pid)
        {
            pcb[i].mask = mask;
            printk("[TASKSET] Task %d set to mask %d\n", pid, mask);
            spin_lock_release(&pcb[i].spinlock);
            break;
        }
        spin_lock_release(&pcb[i].spinlock);
    }
    if(i == NUM_MAX_TASK)
    {
        printl("[TASKSET] Task %d not found\n", pid);
    }
}
