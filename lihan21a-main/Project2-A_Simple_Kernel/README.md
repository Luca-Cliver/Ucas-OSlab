# Project 2: A Simple Kernel

## 实验任务
- task1：任务启动，非抢占式调度
- task2：互斥锁的实现
- task3：系统调用
- task4：定时器中断，实现抢占式调度
- task5：进程的创建，并自行编写测试文件

## 运行方式
```sh
make all     #编译文件，制作image文件
make run     #启动qemu，运行
#上板运行
make floppy  #把镜像写到SD卡里
make minicom #监视串口
```

进入BBL界面之后，输入```loadbootd```启动，运行界面如下,其中第九行是线程执行的次数，用于标定验证第十行主动调度的次数
```
> [print1] This task is to test scheduler. (100)                                
> [print2] This task is to test scheduler. (98)                                 
> [lock1 ] Has acquired lock and running.(48)                                   
> [lock2 ] Applying for a lock.                                                 
> [sleep ] This task is to test sleep. (6)                                      
> [timer ] This is a thread to timing! (19/19462582 seconds).                   
> [thread1] This is in the 1st thread! (37)                                     
> [thread2] This is in the 2nd thread! (37)                                     
thread1 yield time is 10 thread2 yield time is 10 thread1 yield time is 20      
> [tcb_sch] yield times = 10                                                    
--------------         _                                                        
--------------       -=\`\                                                      
-------------   |\ ____\_\__                                                    
------------- =\c`""""""" "`)                                                   
-------------    `~~~~~/ /~~`                                                   
-------------      -==/ /                                                       
-------------        '-'
```

## 设计流程
### 1. 初始化进程
- `init_jmptab`用于将kernel与bios提供的函数记录在跳转表中
- - `init_pcb`具体包括下述功能：
  - 加载用户程序到内存
  - 为用户程序分配用户栈与内核栈空间
  - 修改栈内数据及栈顶指针来模拟“刚受到中断、将从`switch_to`返回”
  - 在PCB中记录用户程序的pid，栈指针等信息
  - 将用户程序的状态标记为ready，并添加到ready队列
  - 令第零号PCB为`pid0_pcb`，并设置`current_running`
  - 设置`tp`寄存器的值为`current_running`（这是为了当发生第一次抢占时，`SAVE_CONTEXT`能找到PCB地址）
- `init_screen`用于初始化屏幕  

### 2. 进程调度
- `do_schedule`用于进程调度,其功能如下：
  - `check_sleeping`用于从sleep队列中唤醒正处于blocked状态的任务（修改其状态为ready，将其添加至ready队列） 
  - 检查是否有进程需要运行，如果没有，则直接返回
  - 调用`schedule`函数，将当前进程标记为`ready`，将这个进程作为pre_running并从`ready`队列中取出下一个进程作为current_running
  - 调用switch_to，实现进程切换
- `switch_to用于进程切换`
  - 调用`SAVE_CONTEXT`保存当前进程的上下文，将当前进程标记为`running`
  - 调用`RESTORE_CONTEXT`恢复下一个进程的上下文
  - 跳转到current_running的`PC`

### 3. 例外的触发和处理
发生例外之后，内核完成下列操作
- `ecall`指令进入内核态
- 进入`exception_handler_entry`
- 进入`SAVE_CONTEXT`，保存上下文到内核栈，并令`sp`指向内核栈
- 进入`interrupt_helper`，根据`scause`调用相应例外处理函数

实际上，这次实验实现的kernel只支持系统调用和定时器中断
- 前者进入`handle_syscall`，根据`a7`寄存器内存储的值调用系统调用表内函数，随后令`sepc`加4
- 后者进入`handle_irq_timer`，更新抢占时刻，并进行下一次调度

### 4.例外的返回

- 从`interrupt_helper`返回，进入`ret_from_exception`
- 进入`RESTORE_CONTEXT`，恢复先前保存的上下文变量，并令`sp`指向用户栈
- `sret`返回用户态

## 关键代码
1）exception_handler_entry函数（位于trap.S） 
```
ENTRY(setup_exception)

  /* TODO: [p2-task3] save exception_handler_entry into STVEC */
  la t0, exception_handler_entry
  csrw CSR_STVEC, t0

  /* TODO: [p2-task4] enable interrupts globally */

  li t0, SR_SIE
  csrw CSR_SSTATUS, t0

  not t0, x0
  csrs CSR_SIE, t0

  ret
ENDPROC(setup_exception) 
```
  1. 保存上下文   2. 调用异常处理函数interrupt_helper（传参）  3.跳入ret_from_exception函数进行收尾处理

2）interrupt_helper（位于irq.c）
```
   void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t scause)
    {
    // TODO: [p2-task3] & [p2-task4] interrupt handler.
    // call corresponding handler by the value of `scause`
    handler_t *handler = (scause >> 63)? irq_table : exc_table;
    scause = scause & 0x7FFFFFFFFFFFFFFF;
    handler[scause](regs, stval, scause);
    }
```
1.用scause中的值判断是例外还是中断选择跳转表   2.得到跳转表中的操作序号   3.执行跳转表中对应的函数

3）handle_syscall函数（位于syscall.c（内核态））
```
    void handle_syscall(regs_context_t *regs, uint64_t interrupt, uint64_t cause)
    {
    /* TODO: [p2-task3] handle syscall exception */
    /**
     * HINT: call syscall function like syscall[fn](arg0, arg1, arg2),
     * and pay attention to the return value and sepc
     */
      regs->sepc = regs->sepc + 4;
    regs->regs[10] = syscall[regs->regs[17]](regs->regs[10],
                                             regs->regs[11],
                                             regs->regs[12],
                                             regs->regs[13],
                                             regs->regs[14]);
    }
```
1.sepc + 4，从而让跳转回用户态时略过系统调用那条指令   2.从通用寄存器存储所在的栈处取出对应参数，从而进行正确的系统调用函数跳转和执行

4）handle_irq_timer（位于irq.c）
```
    void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause)
    {
    // TODO: [p2-task4] clock interrupt handler.
    // Note: use bios_set_timer to reset the timer and remember to reschedule
    bios_set_timer(get_ticks() + 150000);
    do_scheduler();
    }
```
   时钟中断时的处理,重新设置下次时钟中断的时间，然后调度

## 线程的创建和检测
### 创建线程
- 线程的创建函数为thread_create，位于sched.c
- 主线程通过系统调用进入这个函数，传入的参数为创建的线程要用到的信息
- 线程的创建需要创建一个内核线程，然后将线程的栈和寄存器信息填入
- 创建完成后把线程放入就绪队列，等待调用
```
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

    list_add(&new_tcb->list, &ready_queue);
}
```
### 测试程序
- 测试程序要求创建两个子线程，每个子线程对各自一个变量累加，当一个子线程的数据大于另一个线程某个值时，主动调度到另一个线程，并记录下来主动调度的次数，这里可以自己设置阈值，以可以看出现象
- 每个线程的变量作为参数，通过指针传到每一个线程中
- 累加过程中要保证加法操作的原子性，这里用了``` __sync_fetch_and_add```函数，保证累加是原子的
- 主线程中打印出主动调度的次数
```
void thread_1(int *res_t1, int *res_t2, int *yield_times)
{
    for (int i = 0; ; i++)
    {
        while(*res_t1 - *res_t2 >= 4)
        {     
            __sync_fetch_and_add(&flag, 1);
            thread_yield();
        }
        __sync_fetch_and_add(res_t1, 1);
        sys_move_cursor(0,6);
        printf("> [thread1] This is in the 1st thread! (%d)\n", *res_t1);
    }
}
```

### 验证打印结果的正确性
- 想到的方法是调节阈值或者时间片长度，让线程运行的次数和主动调度的次数形成一个比较稳定的关系
- 选择把阈值设置成4，这样可以实现每当一个线程通过时钟中断调度进来，时间片足够其加到阈值，然后调度到另一个线程，剩余的时间片不够另一个进程再加到阈值，会被时钟中断切换
- 这样就可以实现主动调度的次数和时钟中断的次数基本一致，那么主动调度的次数和线程运行的次数就会成一比二的关系
- 为了统计线程运行的次数，选择在内核态定义一个全局变量，每次switch_to之前判断current_running是不是这两个线程，如果是就把其加一
- 最终上板结果与预期一致
```
if(current_running == &tcb[0] || current_running == &tcb[1])
    {
        thread_yield_times++;
        screen_move_cursor(0, 8);
        if(current_running == &tcb[0])
            thread1_yield_times++;
        else
            thread2_yield_times++;
        printk("thread1 yield time is %d thread2 yield time is %d thread1 yield time is %d\n", thread1_yield_times, thread2_yield_times, thread_yield_times);
    }
```

## 总结
这次实验的内容虽然很多，但留出来的时间比较多，因此我的时间相对比较充裕。但由于之前没有接触过这么宏大的项目，因此在上手的过程中还是遇到了不小的困难。印象深刻的是在做系统调用的时候，因为不知道怎么去调用，导致一直卡在系统调用的函数上，最后才意识到是参数的传递问题。实际上后来捋清楚了整个系统调用的过程，这个问题也就简单了起来。
这次实验也让我对理论课上讲的调度、中断等有了更深一步的理解，也让我对内核态有了更深的认识，总的来说受益匪浅。在debug的过程时我也和同学一起讨论过，最终完成实验，再次表示感谢。

