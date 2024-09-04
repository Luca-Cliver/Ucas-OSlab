#include "csr.h"
#include "os/loader.h"
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
#include <sys/syscall.h>

pid_t thread_idx = 0;
extern void ret_from_exception();
extern void do_exit();

void thread_create(pthread_t *thread, void(*thread_entry)(void*), void *arg)
{
    uint64_t cpu_id = get_current_cpu_id();
    thread_idx++;
    for(int i = 0; i < NUM_MAX_TASK; i++)
    {
        if(tcb[i].pid == 0)
        {

            current_running[cpu_id]->thread_idx[current_running[cpu_id]->tcb_num] = thread_idx;
            current_running[cpu_id]->tcb_num++;

            *thread = current_running[cpu_id]->tcb_num;

            tcb[i].core_id = cpu_id;
            tcb[i].mask = current_running[cpu_id]->mask;
            tcb[i].pid = current_running[cpu_id]->pid;
            tcb[i].tid = current_running[cpu_id]->tcb_num;
            tcb[i].status = TASK_READY;
            tcb[i].pgdir = current_running[cpu_id]->pgdir;
            tcb[i].wakeup_time = 0;

            list_init(&tcb[i].list);
            list_init(&tcb[i].wait_list);
            list_init(&tcb[i].page_list);
            list_add(&tcb[i].list, &ready_queue);

            uintptr_t kernel_stack = allocPage(1) + PAGE_SIZE;
            uintptr_t user_stack = alloc_page_helper(USER_STACK_ADDR + (thread_idx-1)*PAGE_SIZE, pa2kva(tcb[i].pgdir));
            uintptr_t entry_point = (uintptr_t)thread_entry;

            regs_context_t *pt_regs = 
                (regs_context_t *)(kernel_stack - sizeof(regs_context_t));

            pt_regs->regs[1] = (reg_t)0x10022;  //ra
            pt_regs->regs[2] = USER_STACK_ADDR + thread_idx*PAGE_SIZE;
            pt_regs->regs[4] = (reg_t)(&tcb[i]);
            pt_regs->regs[10] = (reg_t)arg;
            pt_regs->sepc = (reg_t)entry_point;
            pt_regs->sstatus = (1 << 1);

            switchto_context_t *pt_switchto =
                (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
            
            pt_switchto->regs[0] = (reg_t)ret_from_exception;
            pt_switchto->regs[1] = USER_STACK_ADDR + thread_idx*PAGE_SIZE;

            tcb[i].kernel_sp = kernel_stack - sizeof(regs_context_t) - sizeof(switchto_context_t);
            tcb[i].user_sp = USER_STACK_ADDR + thread_idx*PAGE_SIZE;
            tcb[i].entry_point = entry_point;

            page_t *new_page = (page_t*)kmalloc(sizeof(page_t));
            new_page->pa = kva2pa(user_stack);
            new_page->va = USER_STACK_ADDR + (thread_idx-1)*PAGE_SIZE;
            new_page->in_disk = 1;
            new_page->block_id = 0;
            new_page->owner = tcb[i].pid;
            list_init(&new_page->list);
            list_add(&new_page->list, &tcb[i].page_list);
            tcb[i].p_num = 1;
            break;
        }
    }
}

int thread_join(pthread_t thread)
{
    pcb_t *target_tcb;
    uint64_t cpu_id = get_current_cpu_id();
    for(int i = 0; i < thread_idx; i++)
    {
        if(tcb[i].pid == current_running[cpu_id]->pid && tcb[i].tid == thread)
        {
            if(tcb[i].pid == 0 || tcb[i].pid == 1)
            {
                return -1;
            }
            else 
            {
                target_tcb = &tcb[i];
                if(target_tcb->status != TASK_EXITED)
                {
                    current_running[cpu_id]->status = TASK_BLOCKED;
                    list_add(&(current_running[cpu_id]->list), &(target_tcb->wait_list));
                    do_scheduler();
                    return 1;
                }
            }
        }
    }
}

