# Project 4: 虚拟内存

### 请描述你设计的页表结构、PTE内容、页表大小、页表在内存中的存放位置
页表结构：
```c
typedef struct page{
    uintptr_t pa;
    uintptr_t va;
    list_node_t list;
    uint64_t *pte;
    int in_disk;
    int block_id;
    pid_t owner;
}page_t;
```

PTE内容：
```c
/*
 * PTE format:
 * | XLEN-1  10 | 9             8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0
 *       PFN      reserved for SW   D   A   G   U   X   W   R   V
 */
```

页表大小:一个页

存放位置:在进程的pgdir中

### 请描述用户态进程页表创建设置的过程。并简述内核如何加载一个用户进程，并将其运行在虚拟内存上？
alloc_page_helper()
load_task_img()

### 请描述page fault处理的流程，最好可以提供一些流程伪代码


### 当kill一个持有锁的进程时，你会在kill的实现中进行哪些处理
在进程的数据结构中加入这个进程持有的锁的数量，每次acquire时都将这个值加1，在kill的时候，如果locknum是0，代表没有锁，直接kill,否则就遍历这个进程的锁的数组，释放这个进程的锁的阻塞队列，然后kill

### 你打算如何实现信号量、屏障？请简述你的设计思路或展示伪代码。在使用信号量、屏障时，如果有定时器中断发生，你的内核会做什么处理么？
在main函数中初始化，信号量在启用时会给一个初始值，对于p操作，先用while循环，如果value为0，就阻塞，知道value为正值再调出，对于v操作，直接对这个value加一，如果这个信号量的阻塞队列还有进程，就释放出来第一个进程在,有中断发生的时候，内核会判断是否有进程在等待这个信号量，如果有，就唤醒这个进程，然后将这个信号量的value减一，如果value为0，就阻塞，直到value为正值再调出
屏障则是初始值为0，初始化的时候设置一个目标值goal,当value等于goal的时候，就唤醒所有阻塞在这个信号量的进程，然后将value设置为0

### 请展示你设计的mailbox的数据结构，并简述如何保护mailbox的并发访问？
mailbox在init，send，recv之前都获取一个自旋锁，对于需要阻塞的，在阻塞之前释放锁，再次唤醒时再获得这个锁，当send或recv结束后再release，可以保证正确性

### 你设计的内核是如何让双核正常工作的？请简述双核的使能过程
还没有完全实现，初步设想是让双核用共享的就绪队列，但两个进程有不同的current_running，在做具体的操作时，都要先判断当前的cpu_id。
核间中断，从核启动，还没有实现
