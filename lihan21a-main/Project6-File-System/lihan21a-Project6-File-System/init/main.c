/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *         The kernel's entry, where most of the initialization work is done.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include "os/list.h"
#include "os/smp.h"
#include "pgtable.h"
#include <common.h>
#include <asm.h>
#include <asm/unistd.h>
#include <os/loader.h>
#include <os/irq.h>
#include <os/sched.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/time.h>
#include <os/ioremap.h>
#include <os/net.h>
#include <sys/syscall.h>
#include <screen.h>
//#include <e1000.h>
//#include <plic.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>
#include <os/filesys.h>
#include <riscv.h>

#define VERSION_BUF 50

extern void ret_from_exception();

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];
int mark = 0;

// Task info array
task_info_t tasks[TASK_MAXNUM];
char * debug_name[32];


static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;
    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
    jmptab[SD_WRITE]        = (long (*)())sd_write;
    jmptab[QEMU_LOGGING]    = (long (*)())qemu_logging;
    jmptab[SET_TIMER]       = (long (*)())set_timer;
    jmptab[READ_FDT]        = (long (*)())read_fdt;
    jmptab[MOVE_CURSOR]     = (long (*)())screen_move_cursor;
    jmptab[PRINT]           = (long (*)())printk;
    jmptab[YIELD]           = (long (*)())do_scheduler;
    jmptab[MUTEX_INIT]      = (long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ]       = (long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE]   = (long (*)())do_mutex_lock_release;

    jmptab[WRITE]           = (long (*)())screen_write;
    jmptab[REFLUSH]         = (long (*)())screen_reflush;

    // TODO: [p2-task1] (S-core) initialize system call table.

}

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    int taskinfo_blockid = *(int *)0xffffffc0502001f8;
    bios_sd_read(kva2pa(0xffffffc054000000), 1, taskinfo_blockid);
    //bios_sd_read((uintptr_t)0xffffffc054000000, 2, taskinfo_blockid);
    task_info_t *cur_task = (task_info_t *)0xffffffc054000000;
    for(int i = 0; i < TASK_MAXNUM; i++)
    {
        for(int j = 0; j < TASK_MAXNUM; j++)
        {
            tasks[i].task_name[j] = cur_task[i].task_name[j];
        }
        tasks[i].task_offset = cur_task[i].task_offset;
        tasks[i].task_size = cur_task[i].task_size;
        tasks[i].task_entry = cur_task[i].task_entry;
        tasks[i].task_memsz = cur_task[i].task_memsz;
        debug_name[i] = tasks[i].task_name;
    }
}

/************************************************************/
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
    //pt_regs->sstatus = (reg_t) (SR_SPIE & ~SR_SPP);
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
    pt_switchto->regs[1] = pcb->user_sp;  // sp

}

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    list_init(&ready_queue);
    int j = 0;
    for(int i = 0; i < 18; i++)
    {
        if(i == 0)
        {
            pcb[j].pid = 1;
            pcb[j].core_id = 0;
            pcb[j].status = TASK_READY;
            pcb[j].name = tasks[j].task_name;
            pcb[j].mask = 0;
            pcb[j].pgdir = allocPage(1);
            clear_pgdir(pcb[j].pgdir);
            for(int k = 0; k < PAGE_SIZE; k++)
            {
                k[(char *)pcb[j].pgdir] = 
                    k[(char *)pa2kva(PGDIR_PA)];
            }
            uintptr_t kernel_stack = allocPage(1) + PAGE_SIZE;
            uintptr_t user_stack = USER_STACK_ADDR;
            alloc_page_helper(USER_STACK_ADDR - PAGE_SIZE, pcb[j].pgdir);
            uint64_t entry_point = load_task_img(j, pcb[j].pgdir);
            init_pcb_stack(kernel_stack, user_stack, entry_point, &pcb[j]);
            list_init(&pcb[j].wait_list);
            list_init(&pcb[j].page_list);
            pcb[j].tid = 0;
            pcb[j].tcb_num = 0;
            for(int k = 0; k < 16; k++)
            {
                pcb[j].thread_idx[k] = 0;
            }
            pcb[j].lock_num = 0;
            list_add(&pcb[j].list, &ready_queue);
            pcb[j].pgdir = kva2pa(pcb[j].pgdir);

            j++;
        }
        else 
        {
            //load_task_img(j, pcb[j].pgdir);
            pcb[j].pid = 0;
            pcb[j].status = TASK_EXITED;
            pcb[j].tid = 0;
            pcb[j].tcb_num = 0;
            for(int k = 0; k < 16; k++)
            {
                pcb[j].thread_idx[k] = 0;
            }
            pcb[j].lock_num = 0;
            j++;
        }
    }

    pid0_pcb.pgdir = PGDIR_PA;
    list_init(&pid0_pcb.wait_list);

    /* TODO: [p2-task1] remember to initialize 'current_running' */
    current_running[get_current_cpu_id()] = &pid0_pcb;
    //current_running[get_current_cpu_id()]->status = TASK_BLOCKED;
    //[p2-task4] 把tp置成current_running
    asm volatile("mv tp, %0"
                :"=r"(current_running[get_current_cpu_id()]));
}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    syscall[SYSCALL_SLEEP]         = (long (*)())do_sleep;
    syscall[SYSCALL_YIELD]         = (long (*)())do_scheduler;
    syscall[SYSCALL_WRITE]         = (long (*)())screen_write;
    syscall[SYSCALL_CURSOR]        = (long (*)())screen_move_cursor;
    syscall[SYSCALL_REFLUSH]       = (long (*)())screen_reflush;
    syscall[SYSCALL_CLEAR]         = (long (*)())screen_clear;
    syscall[SYSCALL_GET_TIMEBASE]  = (long (*)())get_time_base;
    syscall[SYSCALL_GET_TICK]      = (long (*)())get_ticks;
    syscall[SYSCALL_LOCK_INIT]     = (long (*)())do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ]      = (long (*)())do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE]  = (long (*)())do_mutex_lock_release;
//    syscall[SYSCALL_THREAD_CREATE] = (long (*)())thread_create;
//    syscall[SYSCALL_THREAD_YIELD]  = (long (*)())thread_yield;

    //[p3-task1]
    syscall[SYSCALL_GET_CHAR]      = (long (*)())bios_getchar;
    syscall[SYSCALL_PUT_CHAR]      = (long (*)())bios_putchar;
    syscall[SYSCALL_PS]            = (long (*)())do_process_show;
    syscall[SYSCALL_CLEAR]         = (long (*)())screen_clear;
    syscall[SYSCALL_KILL]          = (long (*)())do_kill;
    syscall[SYSCALL_EXIT]          = (long (*)())do_exit;
    syscall[SYSCALL_WAITPID]       = (long (*)())do_waitpid;
    syscall[SYSCALL_GETPID]        = (long (*)())do_getpid;
    syscall[SYSCALL_EXEC]          = (long (*)())do_exec;
    syscall[SYSCALL_BACKSPACE]     = (long (*)())do_backspace;

    //[p3-task2]
    syscall[SYSCALL_BARR_INIT]     = (long (*)())do_barrier_init;
    syscall[SYSCALL_BARR_WAIT]     = (long (*)())do_barrier_wait;
    syscall[SYSCALL_BARR_DESTROY]  = (long (*)())do_barrier_destroy;
    syscall[SYSCALL_SEMA_INIT]     = (long (*)())do_semaphore_init;
    syscall[SYSCALL_SEMA_UP]       = (long (*)())do_semaphore_up;
    syscall[SYSCALL_SEMA_DOWN]     = (long (*)())do_semaphore_down;
    syscall[SYSCALL_SEMA_DESTROY]  = (long (*)())do_semaphore_destroy;
    syscall[SYSCALL_MBOX_OPEN]     = (long (*)())do_mbox_open;
    syscall[SYSCALL_MBOX_CLOSE]    = (long (*)())do_mbox_close;
    syscall[SYSCALL_MBOX_SEND]     = (long (*)())do_mbox_send;
    syscall[SYSCALL_MBOX_RECV]     = (long (*)())do_mbox_recv;

    syscall[SYSCALL_TASKSET]       = (long (*)())do_taskset;
    syscall[SYSCALL_TASKSET_BYPID] = (long (*)())do_taskset_bypid;

    syscall[SYSCALL_PTHREAD_CREATE] = (long (*)())thread_create;
    syscall[SYSCALL_PTHREAD_JOIN]   = (long (*)())thread_join;
    syscall[SYSCALL_FORK]          = (long (*)())do_fork;
    syscall[SYSCALL_SHM_GET]       = (long (*)())shm_page_get;
    syscall[SYSCALL_SHM_DT]        = (long (*)())shm_page_dt;

    syscall[SYSCALL_NET_SEND]      = (long (*)())do_net_send;
    syscall[SYSCALL_NET_RECV]      = (long (*)())do_net_recv;
    syscall[SYSCALL_NET_RECV_STREAM] = (long (*)())do_net_recv_stream;

    syscall[SYSCALL_FS_MKFS]       = (long (*)())do_mkfs;
    syscall[SYSCALL_FS_STATFS]     = (long (*)())do_statfs;
    syscall[SYSCALL_FS_MKDIR]      = (long (*)())do_mkdir;
    syscall[SYSCALL_FS_CD]         = (long (*)())do_cd;
    syscall[SYSCALL_FS_LS]         = (long (*)())do_ls;
    syscall[SYSCALL_FS_RMDIR]      = (long (*)())do_rmdir;
    syscall[SYSCALL_FS_TOUCH]      = (long (*)())do_touch;   
    syscall[SYSCALL_FS_CAT]        = (long (*)())do_cat; 
    syscall[SYSCALL_FS_FOPEN]      = (long (*)())do_fopen;
    syscall[SYSCALL_FS_FREAD]      = (long (*)())do_fread;    
    syscall[SYSCALL_FS_FWRITE]     = (long (*)())do_fwrite;
    syscall[SYSCALL_FS_FCLOSE]     = (long (*)())do_fclose; 
    syscall[SYSCALL_FS_LN]         = (long (*)())do_ln;
    syscall[SYSCALL_FS_RM]         = (long (*)())do_rm;
    syscall[SYSCALL_FS_LSEEK]      = (long (*)())do_lseek;  
}
/************************************************************/

void recover_map()
{
    uint64_t va = 0x50200000;
    uint64_t pgdir = pa2kva(PGDIR_PA);
    uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT+2*PPN_BITS)) & ~(~0 << PPN_BITS);
    uint64_t vpn1 = (va >> (NORMAL_PAGE_SHIFT+PPN_BITS)) & ~(~0 << PPN_BITS);
    PTE* pte1 = (PTE*)pgdir+vpn2;
    PTE* pte = (PTE*)pa2kva(get_pa(*pte1) + vpn1);
    *pte1 = 0;
    *pte = 0;
}


//pcb_t * newcore_pcb;
int main(void)
{
    if(get_current_cpu_id() != 0)
    {
        mark = 1;
        //拿锁，初始化
        lock_kernel();
        mark = 1;
        ptr_t kernel_stack = allocPage(1) + PAGE_SIZE;
        kernel_stack -= (sizeof(switchto_context_t) + sizeof(regs_context_t));

        newcore_pcb = (pcb_t *)kernel_stack;
        kernel_stack -= sizeof(pcb_t);
        // 128 bit 对齐
        while(kernel_stack%16){
            kernel_stack--;
        }
        

        char name[] = "core";
        name[4] = '0' + get_current_cpu_id();
        // //strcpy(newcore_pcb->name, name);
        newcore_pcb->name = name;
        newcore_pcb->pid = 0;
        newcore_pcb->tid = 0;
        newcore_pcb->cursor_x = 0;
        newcore_pcb->cursor_y = 0+get_current_cpu_id();
        newcore_pcb->status = TASK_RUNNING;
        newcore_pcb->kernel_sp=kernel_stack;
        newcore_pcb->user_sp=kernel_stack;
        newcore_pcb->mask = 0;
        list_init(&newcore_pcb->list);
        list_init(&newcore_pcb->wait_list);
        newcore_pcb->core_id = get_current_cpu_id();
        newcore_pcb->pgdir = PGDIR_PA;
        
        current_running[get_current_cpu_id()] = newcore_pcb;
        //current_running[get_current_cpu_id()] == &pid0_pcb_promax;
        asm volatile("mv tp, %0"
                     :"=r"(newcore_pcb));
        
        //printk("%s succ", name);
        setup_exception(); 
        // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
        // NOTE: The function of sstatus.sie is different from sie's
        bios_set_timer(get_ticks()+TIMER_INTERVAL);
        printk("> [INIT] CPU %lu initialization succeeded.\n", get_current_cpu_id());

        unlock_kernel();
        enable_interrupt();
        // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
        /*for debug*/
        /*
        printk("CPU %lu: sstatus: %lx sie: %lx stvec: %lx\ne.h.entry: %lx\n",
            get_current_cpu_id(), r_sstatus(), r_sie(), r_stvec(), (uint64_t)exception_handler_entry);
        */
        while (1)
        {
            // If you do non-preemptive scheduling, it's used to surrender control
            // do_scheduler();
            // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
            enable_preempt();
            asm volatile("wfi");
        }
    }
    else 
    {
        // Init jump table provided by kernel and bios(ΦωΦ)
        init_jmptab();

        // Init task information (〃'▽'〃)
        init_task_info();

        // Read CPU frequency (｡•ᴗ-)_
        time_base = bios_read_fdt(TIMEBASE);

        // Read Flatten Device Tree (｡•ᴗ-)_
        // e1000 = (volatile uint8_t *)bios_read_fdt(ETHERNET_ADDR);
        // uint64_t plic_addr = bios_read_fdt(PLIC_ADDR);
        // uint32_t nr_irqs = (uint32_t)bios_read_fdt(NR_IRQS);

        // IOremap
        // plic_addr = (uintptr_t)ioremap((uint64_t)plic_addr, 0x4000 * NORMAL_PAGE_SIZE);
        // e1000 = (uint8_t *)ioremap((uint64_t)e1000, 8 * NORMAL_PAGE_SIZE);

        // Init Process Control Blocks |•'-'•) ✧
        init_pcb();
        // printk("> [INIT] e1000: %lx, plic_addr: %lx, nr_irqs: %lx.\n", e1000, plic_addr, nr_irqs);
        // printk("> [INIT] IOremap initialization succeeded.\n");
        printk("> [INIT] PCB initialization succeeded.\n");


        // Init lock mechanism o(´^｀)o
        init_locks();
        printk("> [INIT] Lock mechanism initialization succeeded.\n");

        // for [p3-task2]
        // Init barrier mechanism
        init_barriers();
        printk("> [INIT] Barrier mechanism initialization succeeded.\n");

        // for [p3-task2]
        // Init condition mechanism
        init_semaphores();
        printk("> [INIT] Semaphore mechanism initialization succeeded.\n");

        // for [p3-task2]
        // Init mailbox mechanism
        init_mbox();
        printk("> [INIT] Mailbox mechanism initialization succeeded.\n");

        // // TODO: [p5-task3] Init plic
        // plic_init(plic_addr, nr_irqs);
        // printk("> [INIT] PLIC initialized successfully. addr = 0x%lx, nr_irqs=0x%x\n", plic_addr, nr_irqs);

        // // Init network device
        // e1000_init();
        // printk("> [INIT] E1000 device initialized successfully.\n");

        // Init interrupt (^_^)
        init_exception();
        printk("> [INIT] Interrupt processing initialization succeeded.\n");

        // Init system call table (0_0)
        init_syscall();
        printk("> [INIT] System call initialized successfully.\n");

        // Init screen (QAQ)
        init_screen();
        printk("> [INIT] SCREEN initialization succeeded.\n");

        init_page();

        // for [p3-task3]唤醒从核
        smp_init();
        wakeup_other_hart();
        lock_kernel();

        while(mark == 0);
        recover_map();
        clear_SIP();         // main core will also receive ipi, thus we clear sip here

        //先检查有没有文件系统，没有就造一个
        bios_sd_read(SUPERBLOCK_ADDR, 1, FS_START);
        superblock_t *sb = (superblock_t *)(pa2kva(SUPERBLOCK_ADDR));
        if(sb->magic != SUPERBLOCK_MAGIC)
        {
            do_mkfs();
        }
        // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
        // NOTE: The function of sstatus.sie is different from sie's
        setup_exception(); 
        // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
        // NOTE: The function of sstatus.sie is different from sie's
        bios_set_timer(get_ticks()+TIMER_INTERVAL);
        printk("> [INIT] CPU %d initialization succeeded.\n", get_current_cpu_id());

        // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
        enable_interrupt();
        unlock_kernel();
        while (1)
        {
            // If you do non-preemptive scheduling, it's used to surrender control
            // do_scheduler();

            // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
            enable_preempt();
            asm volatile("wfi");
        }
    }

    


    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.
    /*p1 task2*/
    /*char keep_input;
    while(1)
    {
        keep_input = bios_getchar();
        if(keep_input == -1)
        {
            ;
        }
        else if(keep_input >= '0' && keep_input <= '9')
        {
            int num = keep_input - '0';
            bios_putchar(keep_input);
        }
        else if(keep_input >= 'a' && keep_input <= 'z' || keep_input >= 'A' && keep_input <= 'Z')
        {
            bios_putchar(keep_input);
        }
        else if (keep_input == 13)
        {
            bios_putstr("\n\r");
        }
        else if (keep_input == 127)
        {
            bios_putstr("\b");
        }
    }*/
    /*p1 task3*/
/*  while(1)
    {
        volatile int input_taskid = 0;
        volatile int legal = 1;
        volatile int enter_addr;
        volatile int x = 0;
        while(1)
        {
            x = port_read_ch();
            while(x == -1)
                x = port_read_ch();
            port_write_ch(x);
            if(x == '\n' || x == '\r')
                break;
            else if(x > '9' || x < '0')
            {
                bios_putstr("Wrong taskid\n\r");
                legal = 0;
                break;
            }
            input_taskid = input_taskid * 10 + x - '0';
        }
        if(legal)
        {
            if(input_taskid > 3)
            {
                bios_putstr("Wrong taskid,too large\n\r");
            }
            else
            {
                bios_putstr("task started\n\r");
                enter_addr = load_task_img(input_taskid);
                asm volatile(
                    "jalr %[addr]"
                    ::[addr] "r" (enter_addr)
                );
                bios_putstr("task finished\n\r");
            }
        }
    }*/
    /*p1 task4*/
 /*  volatile int x = 0;
    char temp_name[MAXNAME];
    int temp_name_len = 0;
    volatile int enter_addr;
    while(1)
    {
        int count_task = 0;
        x = bios_getchar();
        if(x == -1)
        {
            ;
        }
        else if(x == '\r' || x == '\n')
        {
            bios_putchar('\n');
            for(int j = 0; j < TASK_MAXNUM; j++)
            {
                if(strcmp(tasks[j].task_name, temp_name) == 0 && temp_name[0] != '\0' || temp_name[0] == j + '0' && temp_name[1] == '\0')
                {
                    enter_addr = load_task_img(j);
                    bios_putstr("task start\n");
                   asm volatile(
                        "jalr %[addr]"
                        ::[addr] "r" (enter_addr)
                    );
                    bios_putstr("task finish\n");
                    break;
                }
                else
                    count_task++;

                if(count_task == TASK_MAXNUM)
                {
                    bios_putstr("Wrong name\n");
                    count_task = 0;
                }
            }
            for(int j = 0; j < MAXNAME; j++)
            {
                temp_name[j] = '\0';
            }
            temp_name_len = 0;
        }
        else if(x == " ")
        {
            bios_putchar(' ');
        }
        else if(x == 127)
        {
            bios_putchar('\b');
            temp_name_len--;
            temp_name[temp_name_len] = '\0';
        }
        else 
        {
            bios_putchar(x);
            temp_name[temp_name_len++] = x;
        }
    }*/

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)

    return 0;
}

/* For debugging */
// void print_for_cpu1(void)
// {
//     if (get_current_cpu_id() != 0)
//         printk("loc 495\n");
// }

//exec rw 0x12000000 0xa0200000 0x30500000 &