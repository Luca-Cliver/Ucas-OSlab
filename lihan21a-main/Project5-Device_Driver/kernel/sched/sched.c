#include "csr.h"
#include "os/loader.h"
#include "os/net.h"
#include "os/smp.h"
#include "pgtable.h"
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
extern void ret_from_exception();

const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .mask = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack,
};



LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);


/* current running task PCB */
extern pcb_t * volatile current_running[2];
extern pcb_t * volatile pre_running;

/* global process id */
pid_t process_id = 1;

void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs

    /************************************************************/
    // TODO: [p5-task3] Check send/recv queue to unblock PCBs
    /************************************************************/

    // TODO: [p2-task1] Modify the current_running[cpu_id] pointer.
    //bios_set_timer(get_ticks() + 150000);
    
    uint64_t cpu_id = get_current_cpu_id();
    pre_running = current_running[cpu_id];
    check_sleeping();
    check_net_unblock();
    // if(cpu_id == 1)
    // {
    //     printk("sche!!!!!!!core%d\n",cpu_id);
    // }

    if(!pre_running->pid)
    {
        pre_running->status = TASK_BLOCKED;
    }

    if(pre_running->status == TASK_RUNNING || 
       pre_running->status == TASK_READY)
    {
        list_add(&pre_running->list, &ready_queue);
        pre_running->status = TASK_READY;
    }
    //pre_running->core_id = -1;

    list_node_t *temp_list;
    pcb_t *temp_pcb;
    //pcb_t *node = current_running[1-cpu_id];
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

    current_running[cpu_id]->core_id = cpu_id;
    current_running[cpu_id]->status = TASK_RUNNING;
    
    // TODO: [p2-task1] switch_to current_running[cpu_id]
    set_satp(SATP_MODE_SV39, current_running[cpu_id]->pid, (uintptr_t)(current_running[cpu_id]->pgdir) >> 12);
    local_flush_tlb_all();
    switch_to(pre_running, current_running[cpu_id]);
    //screen_reflush();
}


void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running[cpu_id]
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running[cpu_id] is blocked.
    uint64_t cpu_id = get_current_cpu_id();
    current_running[cpu_id] ->status = TASK_BLOCKED;
    current_running[cpu_id] ->wakeup_time = sleep_time + get_timer();
    list_add(&current_running[cpu_id]->list, &sleep_queue);
    do_scheduler();
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    list_add(pcb_node, queue);
    //((pcb_t*)((uint64_t)(pcb_node) - 2*sizeof(reg_t)))->status = TASK_BLOCKED;
    LIST2PCB(pcb_node)->status = TASK_BLOCKED;
    do_scheduler();
}

pcb_t * do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    list_del(pcb_node);
    //((pcb_t*)((uint64_t)(pcb_node) - 2*sizeof(reg_t)))->status = TASK_READY;
    LIST2PCB(pcb_node)->status = TASK_READY;
    list_add(pcb_node, &ready_queue);
    return LIST2PCB(pcb_node);
}

int do_kill(pid_t pid, pthread_t tid)
{
    pcb_t *target_pcb;

    int id_num = (tid == 0) ? process_id : thread_idx;
    pcb_t *dest;
    dest = (tid == 0)? pcb:tcb;

    int i = 0;
    for(i = 0; i < id_num; i++)
    {
        if(dest[i].pid == pid && dest[i].tid == tid)
        {
            if(pid == 0 || pid == 1)
                return 0;
            else 
            {
                target_pcb = &dest[i];

                //从队列中删除
                if(target_pcb->status == TASK_READY || target_pcb->status == TASK_BLOCKED)
                    list_del(&target_pcb->list);

                //释放锁    
                for(i = 0; i < target_pcb->lock_num; i++)
                {
                    if(!list_is_empty(&target_pcb->lock[i]->block_queue))
                    {
                        pcb_t *pcb_to_acquire_lock
                        // = (pcb_t *)((uint64_t)((&(target_pcb->lock[i]->block_queue))->next) - 4*sizeof(reg_t));
                        = LIST2PCB((&(target_pcb->lock[i]->block_queue))->next);
                        pcb_to_acquire_lock->status = TASK_READY;
                        list_del((&(target_pcb->lock[i]->block_queue))->next);
                        list_add(&(pcb_to_acquire_lock->list), &ready_queue);
                    }
                    else
                    {
                        target_pcb->lock[i]->lock.status = UNLOCKED;
                    }
                    
                    target_pcb->lock[i] = NULL;
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
                /*
                //回收内核栈
                uint64_t kernel_stack = target_pcb->kernel_sp & ~(PAGE_SIZE-1);
                freePage(kva2pa(kernel_stack));
                //回收页表项
                list_node_t *temp_page = &(target_pcb->page_list);
                while(!list_is_empty(&(target_pcb->page_list)))
                {
                    page_t *page_to_delete = (page_t *)((uint64_t)(temp_page->next) - 2*sizeof(uint64_t));
                    freePage(page_to_delete->pa);
                    list_del(temp_page->next);
                } 
                */
                target_pcb->status = TASK_EXITED;
                target_pcb->pid = 0;

                do_scheduler();
                return 1;
            }
        }
    }
    return 0;
}

void do_exit(void)
{
    uint64_t cpu_id = get_current_cpu_id();
    do_kill(current_running[cpu_id]->pid, current_running[cpu_id]->tid);
}

int do_waitpid(pid_t pid)
{
    pcb_t *target_pcb;
    uint64_t cpu_id = get_current_cpu_id();
    for(int i = 0; i < process_id; i++)
    {
        if(pcb[i].pid == pid)
        {
            if(pid == 0 || pid == 1)
                return 0;
            else
            {           
                target_pcb = &pcb[i];
                if(pcb[i].status != TASK_EXITED)
                {
                    current_running[cpu_id]->status = TASK_BLOCKED;
                    list_add(&(current_running[cpu_id]->list), &(target_pcb->wait_list));
                    do_scheduler();
                    return 1;
                }
            }
        }
    }
    return 0;
}

void init_target_lock(pcb_t pcb)
{
    for(int i = 0; i < LOCK_NUM; i++)
    {
        pcb.lock[i] = &lock_zero;
    }
}

pid_t do_exec(char *name, int argc, char *argv[])
{
    process_id++;
    int succ_pid = 0;
    int cpu_id = get_current_cpu_id();


    int i = 0;
    for(i = 0; i < NUM_MAX_TASK; i++)
    {
        if(!strcmp(name, tasks[i].task_name))
        {
            for(int j = 0; j < NUM_MAX_TASK; j++)
            {
                if(pcb[j].pid == 0)
                {
                    list_init(&(pcb[j].wait_list));
                    list_init(&(pcb[j].page_list));
                    pcb[j].pid = process_id;
                    pcb[j].tid = 0;
                    pcb[j].status = TASK_READY;
                    pcb[j].name = tasks[i].task_name;
                    pcb[j].mask = current_running[cpu_id]->mask;
                    pcb[j].lock_num = 0;
                    pcb[j].tcb_num = 0;
                    pcb[j].wakeup_time = 0;
                    pcb[j].share_p_num = 0;
                    init_target_lock(pcb[j]);
                    //pcb[j].mask = 2;
                    for(int k = 0; k < 16; k++)
                    {
                        pcb[j].thread_idx[k] = 0;
                    }

                    pcb[j].pgdir = allocPage(1);
                    for(int k = 0; k < PAGE_SIZE; k++)
                    {
                        k[(char *)pcb[j].pgdir] = 
                            k[(char *)pa2kva(PGDIR_PA)];
                    }
                    uint64_t kernel_stack = allocPage(1) + PAGE_SIZE;
                    uint64_t user_stack = USER_STACK_ADDR;
                    uint64_t kuser_stack = alloc_page_helper(USER_STACK_ADDR - PAGE_SIZE, pcb[j].pgdir)+ PAGE_SIZE;
                    uint64_t kuser_stack_pre = kuser_stack;
                    uint64_t argv_base = kuser_stack - argc * sizeof(ptr_t);
                    list_add(&(pcb[j].list), &(ready_queue));
                    uint64_t entry_point = load_task_img(i, pcb[j].pgdir);
                   
                    ptr_t *p1, *p2;
                    p1 = (ptr_t *)argv_base;
                    p2 = p1;
                    for(int k = 0; k < argc; k++)
                    {
                        p2 = (ptr_t *)((uint64_t)p2 - 16);
                        p2 = (ptr_t *)strcpy(p2, argv[k]);
                        *p1 = (uint64_t)(USER_STACK_ADDR - (kuser_stack - (uint64_t)p2));
                        p1 = (ptr_t*)((uint64_t)p1 + sizeof(uint64_t));
                    }
 
                    //128bit对齐
                    uint64_t extra = (kuser_stack - (uint64_t)p2) % 16;
                    if(extra != 0)
                    {
                        p2 = (ptr_t *)((uint64_t)p2 - (16 - extra));
                    }
                    kuser_stack = (uint64_t)p2;
                    //record the sp offset
                    uint64_t arg_offset = kuser_stack_pre - kuser_stack;

                    //初始化内核栈
                    regs_context_t *pt_regs =
                        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));

                    // for (int n = 0; n < 32; n++){
                    //     pt_regs->regs[n] = 0;
                    // }
                    pt_regs->regs[1] = (reg_t)entry_point; //ra
                    pt_regs->regs[2] = (reg_t)user_stack - arg_offset;  //sp
                    pt_regs->regs[4] = (reg_t)(&pcb[j]);
                    pt_regs->regs[10] = (reg_t)argc;
                    pt_regs->regs[11] = USER_STACK_ADDR - argc * sizeof(ptr_t);
                    pt_regs->sepc = (reg_t)entry_point;    //sepc
                    pt_regs->sstatus = (1 << 1);    //sstatus

                    switchto_context_t *pt_switchto =
                        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

                    pt_switchto->regs[0] = (reg_t)ret_from_exception;//ra
                    pt_switchto->regs[1] = user_stack - arg_offset;//sp  
                    pcb[j].kernel_stack_base = kernel_stack;
                    pcb[j].kernel_sp = kernel_stack - sizeof(regs_context_t) - sizeof(switchto_context_t);
                    pcb[j].user_sp   = USER_STACK_ADDR - arg_offset;
                    pcb[j].entry_point = entry_point;
                    pcb[j].pgdir = kva2pa(pcb[j].pgdir);

                    page_t *new_page = (page_t *)kmalloc(sizeof(page_t));
                    new_page->pa = pcb[j].pgdir;
                    new_page->va = kva2pa(pcb[j].pgdir);
                    new_page->owner = pcb[j].pid;
                    new_page->in_disk = 1;
                    list_init(&new_page->list);
                    list_add(&(new_page->list), &(pcb[j].page_list));
                    pcb[j].p_num = 1;

                    succ_pid = pcb[j].pid;
                    break;
                }
            }
            break;
        }
    }
    return succ_pid;
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
            printk("[%d]  %d  RUNNING 0x%d   %s  blocked on core %d\n",
                   live_pcb_num, pcb[i].pid, mask_num, pcb[i].name, pcb[i].core_id);
            live_pcb_num++;
        }
        else if(pcb[i].status == TASK_READY && pcb[i].pid != 0)
        {
            printk("[%d]  %d  READY   0x%d   %s \n",
                   live_pcb_num, pcb[i].pid,  mask_num, pcb[i].name);
            live_pcb_num++;
        }
        else if(pcb[i].status == TASK_BLOCKED && pcb[i].pid != 0)
        {
            printk("[%d]  %d  BLOCKED 0x%d   %s \n",
                   live_pcb_num, pcb[i].pid, mask_num, pcb[i].name);
            live_pcb_num++;
        }
    }
    printk("IDX PID TID STATUS  MASK  \n");
    for(int i = 0; i < thread_idx; i++)
    {
        int mask_num = pcb[i].mask == 0 ? 3 :
                       pcb[i].mask == 1 ? 1 : 2;

        if(tcb[i].status == TASK_RUNNING && tcb[i].pid != 0)
        {
            printk("[%d]  %d %d  RUNNING 0x%d   running on core %d\n",
                   live_pcb_num, tcb[i].pid, tcb[i].tid, mask_num,  pcb[i].core_id);
            live_pcb_num++;
        }
        else if(tcb[i].status == TASK_READY && tcb[i].pid != 0)
        {
            printk("[%d]  %d %d  READY   0x%d  \n",
                   live_pcb_num, tcb[i].pid, tcb[i].tid, mask_num);
            live_pcb_num++;
        }
        else if(tcb[i].status == TASK_BLOCKED && tcb[i].pid != 0)
        {
            printk("[%d]  %d %d  BLOCKED   0x%d \n",
                   live_pcb_num, tcb[i].pid, tcb[i].tid, mask_num);
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
            if(pcb[i].pid == pid_to_taskset)
            {
                pcb[i].mask = mask;
                printk("[TASKSET] Task %d set to mask %d\n", pid_to_taskset, mask);
                break;
            }
        }
    }
    return pid_to_taskset;
}

void do_taskset_bypid(pid_t pid, int mask)
{
    int i = 0;
    for(i = 0; i < NUM_MAX_TASK; i++)
    {
        if(pcb[i].pid == pid)
        {
            pcb[i].mask = mask;
            printk("[TASKSET] Task %d set to mask %d\n", pid, mask);
            break;
        }
    }
    if(i == NUM_MAX_TASK)
    {
        printl("[TASKSET] Task %d not found\n", pid);
    }
}


pid_t do_fork(void)
{
    process_id++;
    uint64_t cpu_id = get_current_cpu_id();

    for(int i = 0; i < NUM_MAX_TASK; i++)
    {
        if(pcb[i].pid == 0)
        {
            pcb[i].pid = process_id;
            pcb[i].tid = 0;
            pcb[i].status = TASK_READY;
            pcb[i].mask = current_running[cpu_id]->mask;
            pcb[i].name = current_running[cpu_id]->name;
            pcb[i].lock_num = 0;
            pcb[i].tcb_num = 0;
            pcb[i].wakeup_time = 0;
            pcb[i].share_p_num = 0;
            pcb[i].pgdir = allocPage(1);
            // for(int k = 0; k < PAGE_SIZE; k++)
            // {
            //     k[(char *)pcb[i].pgdir] = 
            //     k[(char *)pa2kva(PGDIR_PA)];
            // }
            share_pgtable(pcb[i].pgdir, pa2kva(PGDIR_PA));
            

            list_init(&pcb[i].wait_list);
            list_init(&pcb[i].page_list);
            list_add(&pcb[i].list, &ready_queue);


            pcb[i].kernel_stack_base = allocPage(1) + PAGE_SIZE; 
            pcb[i].user_sp = current_running[cpu_id]->user_sp;
            pcb[i].entry_point = current_running[cpu_id]->entry_point;
            pcb[i].pgdir = kva2pa(pcb[i].pgdir);

            //初始化内核栈
            regs_context_t *pt_regs =
                    (regs_context_t *)(pcb[i].kernel_stack_base - sizeof(regs_context_t));
            memcpy((uint8_t * )pcb[i].kernel_stack_base- PAGE_SIZE,
                    (uint8_t *)(current_running[cpu_id]->kernel_stack_base - PAGE_SIZE),
                    PAGE_SIZE);
            pt_regs->regs[4] = (reg_t)&pcb[i];
            pt_regs->regs[10] = (reg_t)0; //a0,置成0

            switchto_context_t *pt_switchto =
                        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

            pt_switchto->regs[0] = (reg_t)ret_from_exception;//ra
            pt_switchto->regs[1] = pcb[i].user_sp;//sp  

            pcb[i].kernel_sp = pcb[i].kernel_stack_base - sizeof(regs_context_t) - sizeof(switchto_context_t);


            // PTE *pt2 = (PTE *)pa2kva(current_running[cpu_id]->pgdir);
            // for(int j = 0; j < PAGE_SIZE/sizeof(PTE); j++)
            // {
            //     if(pt2[j] == 0)
            //         continue;
            //     PTE *pt1 = (PTE *)pa2kva(get_pa(pt2[j]));
            //     for(int k = 0; k < PPN_BITS/sizeof(PTE); k++)
            //     {
            //         if(pt1[k] == 0)
            //             continue;
            //         PTE *pt0 = (PTE *)pa2kva(get_pa(pt1[k]));
            //         for(int l = 0; l < PPN_BITS/sizeof(PTE); l++)
            //         {
            //             if(pt0[l] == 0)
            //                 continue;
            //             //pt0[l] &= ~_PAGE_WRITE;
            //         }
            //     }
            // }
            copy_on_write((PTE *)pa2kva(pcb[i].pgdir) , (PTE *)pa2kva(current_running[cpu_id]->pgdir));
            //share_pgtable(pcb[i].pgdir, pa2kva(current_running[cpu_id]->pgdir));
            
            return pcb[i].pid;
        }
    }
    return 0;
}
