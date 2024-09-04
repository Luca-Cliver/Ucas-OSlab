#include "os/smp.h"
#include "type.h"
#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <assert.h> 
#include <atomic.h>
#include <os/string.h>

mutex_lock_t mlocks[LOCK_NUM];
barrier_t barrier[BARRIER_NUM];
condition_t cond[CONDITION_NUM];
mailbox_t mbox[MBOX_NUM];
semaphore_t sema[SEMAPHORE_NUM];

void init_locks(void)
{
    /* TODO: [p2-task2] initialize mlocks */
    for(int i = 0; i < LOCK_NUM; i++)
    {
        mlocks[i].lock.status = UNLOCKED;
        list_init(&(mlocks[i].block_queue));
    }
}

void spin_lock_init(spin_lock_t *lock)
{
    /* TODO: [p2-task2] initialize spin lock */
    atomic_swap(UNLOCKED, (ptr_t)&lock->status);
    lock->owner = -1;
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] try to acquire spin lock */
    if(atomic_swap(LOCKED, (ptr_t)&lock->status))
        return 0;
    else
    {
        lock->owner = get_current_cpu_id();
        return 1;
    }
}

void spin_lock_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] acquire spin lock */
    while(atomic_swap(LOCKED, (ptr_t)&lock->status))
        ;
    lock->owner=get_current_cpu_id();
}

void spin_lock_release(spin_lock_t *lock)
{
    /* TODO: [p2-task2] release spin lock */
    lock->owner=-1;
    atomic_swap(UNLOCKED, (ptr_t)&lock->status);
}

int do_mutex_lock_init(int key)
{
    /* TODO: [p2-task2] initialize mutex lock */
    int mlocks_idx = key % LOCK_NUM;
    return mlocks_idx;
}

void do_mutex_lock_acquire(int mlock_idx)
{
    /* TODO: [p2-task2] acquire mutex lock */
    uint64_t cpu_id = get_current_cpu_id();
    if(mlocks[mlock_idx].lock.status == UNLOCKED)
    {
        mlocks[mlock_idx].lock.status = LOCKED;
        current_running[cpu_id]->lock[current_running[cpu_id]->lock_num] = &(mlocks[mlock_idx]);
        current_running[cpu_id]->lock_num++;
    }
    else 
    {
        do_block(&(current_running[cpu_id]->list), &(mlocks[mlock_idx].block_queue));
    }
}

void do_mutex_lock_release(int mlock_idx)
{
    uint64_t cpu_id = get_current_cpu_id();
    for(int i = 0; i < current_running[cpu_id]->lock_num; i++)
    {
        if(current_running[cpu_id]->lock[i] == &(mlocks[mlock_idx]))
        {
            for(int j = i; j < current_running[cpu_id]->lock_num - 1; j++)
            {
                current_running[cpu_id]->lock[j] = current_running[cpu_id]->lock[j + 1];
            }
            current_running[cpu_id]->lock_num--;
            break;
        }
    }
    /* TODO: [p2-task2] release mutex lock */
    if(list_is_empty(&(mlocks[mlock_idx].block_queue)))
    {
        mlocks[mlock_idx].lock.status = UNLOCKED;
    }
    else 
    {
        do_unblock(mlocks[mlock_idx].block_queue.next);
    }
}


void init_barriers()
{
    for(int i = 0; i < BARRIER_NUM; i++)
    {
        barrier[i].goal = 0;
        barrier[i].now_num = 0;
        spin_lock_init(&(barrier[i].lock));
        barrier[i].key = 0;
        list_init(&(barrier[i].block_queue));
    }
}

int do_barrier_init(int key, int goal)
{
    int barrier_id = key % BARRIER_NUM;
    barrier[barrier_id].goal = goal;
    barrier[barrier_id].now_num = 0;
    list_init(&(barrier[barrier_id].block_queue));
    return barrier_id; 
}

void do_barrier_wait(int bar_idx)
{
    uint64_t cpu_id = get_current_cpu_id();
    if(++barrier[bar_idx].now_num >= barrier[bar_idx].goal)
    {
        list_node_t *temp;
        while(!list_is_empty(&(barrier[bar_idx].block_queue)))
        {
            temp = barrier[bar_idx].block_queue.next;
            do_unblock(temp);
        }
        barrier[bar_idx].now_num = 0;
    }
    else
    {
        do_block(&(current_running[cpu_id]->list), &barrier[bar_idx].block_queue);
    }
}

void do_barrier_destroy(int bar_idx)
{
    barrier[bar_idx].goal = 0;
    barrier[bar_idx].now_num = 0;
    while(!list_is_empty(&(barrier[bar_idx].block_queue)))
    {
        do_unblock(barrier[bar_idx].block_queue.next);
    }
}

void init_semaphores(void)
{
    for(int i = 0; i < SEMAPHORE_NUM; i++)
    {
        spin_lock_init(&(sema[i].lock));
        sema[i].value = 0;
        sema[i].key = 0;
        list_init(&(sema[i].block_queue));
    }
}

int do_semaphore_init(int key, int init)
{
    int sema_id = key % SEMAPHORE_NUM;
    sema[sema_id].value = init;
    list_init(&(sema[sema_id].block_queue));
    return sema_id;
}

void do_semaphore_down(int sema_idx)
{
    while(sema[sema_idx].value <= 0)
    {
        do_block(&(current_running[get_current_cpu_id()]->list), &(sema[sema_idx].block_queue));
    }
    
    sema[sema_idx].value--;
    return;
    
    // if(sema[sema_idx].value > 0)
    // {
    //     sema[sema_idx].value--;
    //     return;
    // }
    // else 
    // {
    //     do_block(&(current_running[get_current_cpu_id()]->list), &(sema[sema_idx].block_queue));
    // }
}

void do_semaphore_up(int sema_idx)
{
    sema[sema_idx].value++;
    if(!list_is_empty(&(sema[sema_idx].block_queue)))
    {
        do_unblock(sema[sema_idx].block_queue.next);
    }
}

void do_semaphore_destroy(int sema_idx)
{
    while(!list_is_empty(&(sema[sema_idx].block_queue)))
    {
        do_unblock(sema[sema_idx].block_queue.next);
    }
    sema[sema_idx].value = 0;
}

void init_mbox()
{
    for(int i = 0; i < MBOX_NUM; i++)
    {
        spin_lock_init(&(mbox[i].lock));
        mbox[i].start = 0;
        mbox[i].end = 0;
        list_init(&(mbox[i].block_queue));
        mbox[i].handle_num = 0;
    }
}

int do_mbox_open(char *name)
{
    for(int i = 0; i < MBOX_NUM; i++)
    {
        spin_lock_acquire(&(mbox[i].lock));
        if(strcmp(name, mbox[i].name) == 0)
        {
            mbox[i].handle_num++;
            spin_lock_release(&(mbox[i].lock));
            return i;
        }
        spin_lock_release(&(mbox[i].lock));
    }

    for(int i = 0; i < MBOX_NUM; i++)
    {
        spin_lock_acquire(&(mbox[i].lock));
        if(mbox[i].handle_num == 0)
        {
            strcpy(mbox[i].name, name);
            mbox[i].handle_num++;
            assert(mbox[i].start==0);
            assert(mbox[i].end==0);
            assert(list_is_empty(&mbox[i].block_queue));
            spin_lock_release(&(mbox[i].lock));
            return i;
        }
        spin_lock_release(&(mbox[i].lock));
    }
    return -1;
}

void do_mbox_close(int mbox_idx)
{
    mbox[mbox_idx].handle_num = 0;
    while(!list_is_empty(&(mbox[mbox_idx].block_queue)))
    {
        do_unblock(mbox[mbox_idx].block_queue.next);
    }
    mbox[mbox_idx].start = 0;
    mbox[mbox_idx].end = 0;
}

int do_mbox_send(int mbox_idx, void *msg, int msg_length)
{
    spin_lock_acquire(&(mbox[mbox_idx].lock));
    if(mbox_idx < 0 || mbox_idx >= MBOX_NUM)
    {
        spin_lock_release(&(mbox[mbox_idx].lock));
        return -1;
    }

    int ret = 0;
    while((mbox[mbox_idx].end - mbox[mbox_idx].start + msg_length) > MAX_MBOX_LENGTH)
    {
        ret++;
        spin_lock_release(&(mbox[mbox_idx].lock));
        do_block(&(current_running[get_current_cpu_id()]->list), &(mbox[mbox_idx].block_queue));
        spin_lock_acquire(&(mbox[mbox_idx].lock));
    }


    for(int i = 0; i < msg_length; i++)
    {
        mbox[mbox_idx].msg[mbox[mbox_idx].end%(MAX_MBOX_LENGTH+1)] = i[((char *)msg)];
        mbox[mbox_idx].end++;
    }

    while(!list_is_empty(&(mbox[mbox_idx].block_queue)))
    {
        do_unblock(mbox[mbox_idx].block_queue.next);
    }

    spin_lock_release(&(mbox[mbox_idx].lock));
    return ret;
}

int do_mbox_recv(int mbox_idx, void *msg, int msg_length)
{
    spin_lock_acquire(&(mbox[mbox_idx].lock));
    if(mbox_idx < 0 || mbox_idx >= MBOX_NUM)
    {
        spin_lock_release(&(mbox[mbox_idx].lock));
        return -1;
    }

    int ret = 0;
    while(mbox[mbox_idx].end - mbox[mbox_idx].start < msg_length)
    {
        ret++;
        spin_lock_release(&(mbox[mbox_idx].lock));
        do_block(&(current_running[get_current_cpu_id()]->list), &(mbox[mbox_idx].block_queue));
        spin_lock_acquire(&(mbox[mbox_idx].lock));
    }

    for(int i = 0; i < msg_length; i++)
    {
        i[((char *)msg)] = mbox[mbox_idx].msg[mbox[mbox_idx].start%(MAX_MBOX_LENGTH+1)];
        mbox[mbox_idx].start++;
    }

    while(!list_is_empty(&(mbox[mbox_idx].block_queue)))
    {
        do_unblock(mbox[mbox_idx].block_queue.next);
    }
    spin_lock_release(&(mbox[mbox_idx].lock));
    return ret;
}


