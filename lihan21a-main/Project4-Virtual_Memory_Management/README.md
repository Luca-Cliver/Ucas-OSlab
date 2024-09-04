# Project 4: Virtual_Memory_Management

## 实验任务
- 任务一 启用虚拟内存机制进入内核，实现内存隔离机制，从 SD 卡中加载用户程序，使用户进程可以使用虚拟地址访问内存。
- 任务二 实现缺页处理程序，发生缺页中断时自动分配物理页面。并验证之前的进程锁实现在虚存开启的情况下依然有效。
- 任务三 实现换页机制，在物理内存不够时或者当物理页框不在内存中时，将数据与SD 卡之间进行交换，从而支持将虚拟地址空间进一步扩大。
- 任务四 实现虚存下的多线程管理，使得一个进程可以用多个线程分别执行不同的任务。
- 任务五 实现共享内存机制，使得两个进程可以使用共享的一块物理内存，并用共享内存机制完成进程通信。
- 任务六 实现 copy-on-write 策略，并编写带有 fork 功能的测试程序进行验证。

## 运行方式
```sh
make all     #编译文件，制作image文件
make run     #启动qemu，运行
make run-smp #启动qemu，运行，开启多核
#上板运行
make floppy  #把镜像写到SD卡里
make minicom #监视串口
```

进入BBL界面之后，输入```loadbootd```启动单核，输入```loadbootm```启动双核，shell界面如下。运行界面如下,其中第九行是线程执行的次数，用于标定验证第十行主动调度的次数
```
> [INIT] SCREEN initialization succeeded.                                       
> [INIT] CPU 1 initialization succeeded.                                        
                                                                                
                                                                                
                                                                                
                                                                                
                                                                                
                                                                                
                                                                                
                                                                                
                                                                                
                                                                                
                                                                                
                                                                                
                                                                                
                                                                                
                                                                                
                                                                                
                                                                                
                                                                                
------------------- COMMAND -------------------                                 
> root@UCAS_OS:                        



```


## 设计流程
### 1. 开启虚存机制
- bootblock内加载内核之后，跳转到start.s，屏蔽中断，然后进入boot.c。
- 主核按照4MB模式吧虚存的内核高地址部分映射到物理地址的位置，主核建立映射之后，从核只需要复用建立好的页表开启
- 进入main.c后，设置一个flag，等到从核进入main函数后，主核取消映射，这样可以保证从核成功开启虚存模式

### 2. 加载用户进程
- createimage里加上文件的pmesz，在内核加载用户程序时需要用到
- 复用init_task_info,把用户程序的信息读进来，但要把之前的物理地址改成虚拟地址
- 修改load_task_img，为了保证安全，选择一次load进来8个扇区，也就是一个页大小，并用alloc_page_helper分配内存
- 实现alloc_page_helper，获取确定的虚拟地址一处的页大小的内存，然后将其映射到物理地址的函数，然后返回虚拟地址

### 3. 初始化进程
- 首先代码段都要load到0x10000开头的位置，内核栈直接放在内核高地址处，用户栈放在0xf00010000处
- 主要是对栈的变更，高地址空间实际上是存储了所有进程的页表的，它通过线性映射直接映射到物理地址空间，因此要在高地址写入
- 传参的时候要在虚拟地址里传入，然后把用户栈设置成这个虚拟地址映射的物理地址，这里不同的进程可以使用相同的用户栈，因为传参只需要传一次。


### 4.缺页处理和换页机制
- 触发缺页时，有三种情况，一种是已经有页，但没有权限，一种是之前分配过页，但被换入到了磁盘里，一种没有页
- 首先看是不是权限异常，判定A位和D位
- 如果不是权限异常，那么就先查找磁盘，如果在磁盘里有这个页，就把这个页从磁盘里换出来
- 如果都不是，就分配一个页进来
- 对于换页的算法，这里采用了最安全的FIFO算法，我给pcb加上了一个page链表，并设置了一个进程同一时刻能拥有的页的最大的数量。每次把页换入时，换入最前面的，但暂时并不把其从链表中删除，而是先置上标志位，等需要再换入它时，取消这个标志位，并把它移到链表最后。这样就不需要再单独设置一个链表，每次只需要查询这一个链表即可。

### 5. 创建线程
- 线程和主进程共用同一个页表，不需要再重新load代码段
- 内核栈分配和主进程用同样的策略，用户栈则是往后分配
- 入口程序由用户程序作为参数传到内核里，直接把它置到sepc即可
- 由于线程没有经过crt0.S的代码，因此需要手动设置线程退出的地址，这里我采用一个魔数，把tramfream的ra置成crt0.S的call sys_exit的地址

### 6. 共享内存
- 把共享的内存当成共享资源，设置一个数据结构管理，结构体中记录共享页的物理地址、线程数目和对应的虚地址
- 每次根据传进来的key查找共享页
- 每次有线程获取共享页时就进行映射，把线程相关信息记录下来，释放时做相反的过程

### 7. copy on write机制和fork
- 这个功能没能实现
- fork时子进程赋值父进程的栈信息，但需要把a0修改成0，这一部分应该是已经完成了
- 复制页表的时候需要一级一级查找，但没能调出来

## 关键代码
1）alloc_page_helper(在mm.c) 
```c
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir)
{
    // TODO [P4-task1] alloc_page_helper:
    va = va & VA_MASK;
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) & ~(~0 << PPN_BITS);
    uint64_t vpn1 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & ~(~0 << PPN_BITS);
    uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + 2 * PPN_BITS)) & ~(~0 << PPN_BITS);

    PTE *pte0 = NULL;
    PTE *pte1 = NULL;
    PTE *pte2 = NULL;
    pte2 = (PTE *)pgdir + vpn2;

    /*无效时*/
    if(((*pte2) & 0x1) == 0)
    {
        ptr_t pt1_va = allocPage(1);
        uintptr_t pt1_pa = kva2pa(pt1_va);
        *pte2 = (pt1_pa >> NORMAL_PAGE_SHIFT) << _PAGE_PFN_SHIFT;
        set_attribute(pte2, _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(*pte2)));
        pte1 = (PTE *)pt1_va + vpn1; 
    }
    else/*有效时*/
    {
        ptr_t pt1_pa = (*pte2 >> _PAGE_PFN_SHIFT) << NORMAL_PAGE_SHIFT;
        uintptr_t pt1_va = pa2kva(pt1_pa);
        pte1 = (PTE *)pt1_va + vpn1;
    }

    if(((*pte1) & 0x1) == 0)
    {
        ptr_t pt0_va = allocPage(1);
        uintptr_t pt0_pa = kva2pa(pt0_va);
        *pte1 = (pt0_pa >> NORMAL_PAGE_SHIFT) << _PAGE_PFN_SHIFT;
        set_attribute(pte1, _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(*pte1)));
        pte0 = (PTE *)pt0_va + vpn0;
    }
    else 
    {
        ptr_t pt0_pa = (*pte1 >> _PAGE_PFN_SHIFT) << NORMAL_PAGE_SHIFT;
        uintptr_t pt0_va = pa2kva(pt0_pa);
        pte0 = (PTE *)pt0_va + vpn0;
    }

    if(((*pte0) & 0x1) != 0)
        return 0;

    ptr_t va_ = allocPage(1);
    uintptr_t pa_ = kva2pa(va_);
    set_pfn(pte0, pa_ >> NORMAL_PAGE_SHIFT);

    set_attribute(pte0, _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_USER
                  | _PAGE_ACCESSED | _PAGE_DIRTY);
    
    return va_;
}
```

2）2）load_task_img(loader.c中)
```c
uint64_t load_task_img(int taskid, uint64_t pgdir)
{
    int needed_page_num = (tasks[taskid].task_memsz - 1 + PAGE_SIZE) / PAGE_SIZE;
    uint64_t enter_point = tasks[taskid].task_entry;
    int block_id = tasks[taskid].task_offset / SECTOR_SIZE;
    static char temp_sdread[2*PAGE_SIZE];
    uint64_t kva_context = 0;
    for(int i = 0; i < needed_page_num; i++)
    {
        kva_context = alloc_page_helper(enter_point + i*PAGE_SIZE, pgdir);
        bios_sd_read(kva2pa((uintptr_t)temp_sdread), 16, block_id);
        memcpy(
            (uint8_t *)(kva_context),
            (uint8_t *)(temp_sdread + tasks[taskid].task_offset%SECTOR_SIZE),
            PAGE_SIZE
        );
        block_id += 8;
    }    
     
    return enter_point;
    
}
```

3）do_exec(sched.c中)
```c
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
```

4）pthread_create(thread.c中)
```c
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
```

## 实验总结
这次实验保持了一贯的难度，需要对虚拟内存有足够的理解。我在写的时候在S-core遇到的问题最大，首先是进入内核之前，需要开启虚存机制。进入后需要读取用户进程，并给用户进程分配页表，建立映射。这一部分工作量很大，也出了不少bug。之后的任务相对比较顺利，但最后由于时间原因，没能把fork调试出来，比较的遗憾。