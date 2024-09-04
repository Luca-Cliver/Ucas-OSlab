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
#include <sys/syscall.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>

#define VERSION_BUF 50

extern void ret_from_exception();

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

// Task info array
task_info_t tasks[TASK_MAXNUM];


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
    int taskinfo_blockid = *(int *)0x502001f8;
    bios_sd_read(0x50400000, 1, taskinfo_blockid);

    task_info_t *cur_task = (task_info_t *)0x50400000;
    for(int i = 0; i < TASK_MAXNUM; ++i)
    {
        for(int j = 0; j < TASK_MAXNUM; ++j)
        {
            tasks[i].task_name[j] = cur_task[i].task_name[j];
        }
        tasks[i].task_offset = cur_task[i].task_offset;
        tasks[i].task_size = cur_task[i].task_size;
        tasks[i].task_entry = cur_task[i].task_entry;
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
    pt_regs->regs[1] = entry_point;  //ra
    pt_regs->regs[2] = user_stack;   //sp
    pt_regs->regs[4] = pcb;

    pt_regs->sepc = entry_point;
    pt_regs->sstatus = (reg_t) (SR_SPIE & ~SR_SPP);
    //pt_regs->sstatus = (1 << 1);
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

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    list_init(&ready_queue);
    int j = 0;
    for(int i = 0; i < 8; i++)
    {
//        if(i == 2  || i == 5)
 //       {
            pcb[j].pid = process_id++;
            pcb[j].status = TASK_READY;
            pcb[j].tid = 0;
            pcb[j].tcb_num = 0;
            pcb[j].thread_idx = 0;
            list_add(&pcb[j].list, &ready_queue);

            ptr_t kernel_stack = allocKernelPage(1) + PAGE_SIZE;
            ptr_t user_stack = allocUserPage(1) + PAGE_SIZE;
            uint64_t entry_point = load_task_img(i);
            init_pcb_stack(kernel_stack, user_stack, entry_point, &pcb[j]);
            j++;
//        }
    }
    /* TODO: [p2-task1] remember to initialize 'current_running' */
    current_running = &pid0_pcb;
    current_running->status = TASK_BLOCKED;
    //[p2-task4] 把tp置成current_running
    asm volatile("mv tp, %0"
                :"=r"(current_running));
}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    syscall[SYSCALL_SLEEP]         = (long (*)())do_sleep;
    syscall[SYSCALL_YIELD]         = (long (*)())do_scheduler;
    syscall[SYSCALL_WRITE]         = (long (*)())screen_write;
    syscall[SYSCALL_CURSOR]        = (long (*)())screen_move_cursor;
    syscall[SYSCALL_REFLUSH]       = (long (*)())screen_reflush;
    syscall[SYSCALL_GET_TIMEBASE]  = (long (*)())get_time_base;
    syscall[SYSCALL_GET_TICK]      = (long (*)())get_ticks;
    syscall[SYSCALL_LOCK_INIT]     = (long (*)())do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ]      = (long (*)())do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE]  = (long (*)())do_mutex_lock_release;
    syscall[SYSCALL_THREAD_CREATE] = (long (*)())thread_create;
    syscall[SYSCALL_THREAD_YIELD]   = (long (*)())thread_yield;
}
/************************************************************/

int main(void)
{
    // Check whether .bss section is set to zero
//    int check = bss_check();

    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Init Process Control Blocks |•'-'•) ✧
    init_pcb();
    printk("> [INIT] PCB initialization succeeded.\n");

    // Read CPU frequency (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);

    // Init lock mechanism o(´^｀)o
    init_locks();
    printk("> [INIT] Lock mechanism initialization succeeded.\n");

    // Init interrupt (^_^)
    init_exception();
    printk("> [INIT] Interrupt processing initialization succeeded.\n");

    // Init system call table (0_0)
    init_syscall();
    printk("> [INIT] System call initialized successfully.\n");

    // Init screen (QAQ)
    init_screen();
    printk("> [INIT] SCREEN initialization succeeded.\n");

    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
    bios_set_timer(get_ticks() + TIMER_INTERVAL);
    //do_scheduler();
    enable_interrupt();

    while (1)
    {
        // If you do non-preemptive scheduling, it's used to surrender control
        //do_scheduler();

        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        enable_preempt();
        asm volatile("wfi");
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
