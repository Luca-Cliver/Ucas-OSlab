#include "os/kernel.h"
#include "os/list.h"
#include "os/sched.h"
#include "os/smp.h"
#include "pgtable.h"
#include "type.h"
#include <os/mm.h>
#include <os/string.h>
#include <assert.h>
//#include <stdint.h>

// NOTE: A/C-core
static ptr_t kernMemCurr = FREEMEM_KERNEL;
LIST_HEAD(free_page_list);
int swap_block_id = 10000;
int print_loc = 10;

#define MAX_SHM_NUM 30
#define SHARE_P_MEM_BASE 0x57000000
#define SHARE_U_MEM_BASE 0xf60000000
share_t share[MAX_SHM_NUM];

void init_page()
{
    list_init(&free_page_list);
    for(int i = 0; i < MAX_SHM_NUM; i++)
    {
        share[i].num = 0;
        share[i].used = 0;
        for(int j = 0; j < 16; j++)
            share[i].va[j] = 0;
    }
}

ptr_t allocPage(int numPage)
{
    if(!list_is_empty(&free_page_list) && numPage == 1)
    {
            page_t *fp = (page_t *)((uint64_t)(free_page_list.next) - 2*sizeof(uint64_t));
            uintptr_t kva = pa2kva(fp->pa);
            for(int i = 0; i < PAGE_SIZE; i++)
            {
                i[(char *)kva] = 0;
            }
            list_del(&fp->list);
            return kva;
    }
    // align PAGE_SIZE
    else
    {
        ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
        kernMemCurr = ret + numPage * PAGE_SIZE;
        return ret;
    }
}

// NOTE: Only need for S-core to alloc 2MB large page
#ifdef S_CORE
static ptr_t largePageMemCurr = LARGE_PAGE_FREEMEM;
ptr_t allocLargePage(int numPage)
{
    // align LARGE_PAGE_SIZE
    ptr_t ret = ROUND(largePageMemCurr, LARGE_PAGE_SIZE);
    largePageMemCurr = ret + numPage * LARGE_PAGE_SIZE;
    return ret;    
}
#endif

void map_page(uintptr_t va, uintptr_t pa, PTE *pgdir)
{
    printl("get in map_page");
    va = va & VA_MASK;
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) & ~(~0 << PPN_BITS);
    uint64_t vpn1 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & ~(~0 << PPN_BITS);
    uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + 2 * PPN_BITS)) & ~(~0 << PPN_BITS);

    PTE *pte2 = (PTE *)pgdir;
    if(!get_attribute(pte2[vpn2], _PAGE_PRESENT))
    {
        ptr_t pt1_va = allocPage(1);
        uintptr_t pt1_pa = kva2pa(pt1_va);
        set_pfn(&pte2[vpn2], pt1_pa >> NORMAL_PAGE_SHIFT);
        set_attribute(&pte2[vpn2], _PAGE_PRESENT /*| _PAGE_USER*/);
        clear_pgdir(pa2kva(get_pa(pte2[vpn2])));
    }

    PTE *pte1 = (PTE *)pa2kva(get_pa(pte2[vpn2]));
    if(!get_attribute(pte1[vpn1], _PAGE_PRESENT))
    {
        ptr_t pt0_va = allocPage(1);
        uintptr_t pt0_pa = kva2pa(pt0_va);
        set_pfn(&pte1[vpn1], pt0_pa >> NORMAL_PAGE_SHIFT);
        set_attribute(&pte1[vpn1], _PAGE_PRESENT /*| _PAGE_USER*/);
        clear_pgdir(pa2kva(get_pa(pte1[vpn1])));
    }

    PTE *pte0 = (PTE *)pa2kva(get_pa(pte1[vpn1]));
    assert(!get_attribute(pte0[vpn0], _PAGE_PRESENT));
    set_pfn(&pte0[vpn0], pa >> NORMAL_PAGE_SHIFT);
    set_attribute(&pte0[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | 
                                /*_PAGE_USER |*/ _PAGE_ACCESSED | _PAGE_DIRTY);
    local_flush_tlb_all();
    printl("get out map_page");                          
}

int swap_in(pcb_t *owner_pcb)
{
    if(!list_is_empty(&(owner_pcb->page_list)))
    {
        list_node_t *node = owner_pcb->page_list.next;
        page_t *page_to_in = NULL;
        while(node != &(owner_pcb->page_list))
        {
            page_to_in = LIST2PAGE(node);

            uintptr_t *pte = get_pte(page_to_in->va, pa2kva(owner_pcb->pgdir));

            //找到最久的，不在磁盘里的，不要把它从队列里面删除，把有效位和拉低，存到磁盘里，是FIFO实现
            if(!page_to_in->in_disk)
            {
                bios_sd_write(page_to_in->pa, 8, swap_block_id);
                //V位拉低
                *pte &= ~((uint64_t)0x1);
                printk("store to disk, pa = %lx\n", page_to_in->va);
                page_to_in->block_id = swap_block_id;
                swap_block_id += 8;
                page_to_in->in_disk = 1;
                page_to_in->pte = pte;
                owner_pcb->p_num--;
                return 1;
            }
            node = node->next;
        }
    }
    return 0;
}

int swap_out(pcb_t *pcb, uintptr_t va)
{
    list_node_t *node = pcb->page_list.next;
    page_t *page_to_out = NULL;

    while(node != &(pcb->page_list))
    {
        page_to_out = LIST2PAGE(node);

        if(va == page_to_out->va && page_to_out->in_disk == 1)
        {
            //如果页数量已经到3，先踢出去一个
            if(pcb->p_num >= 3)
                swap_in(pcb);

            bios_sd_read(page_to_out->pa, 8, page_to_out->block_id);
            printk("load to mem, pa = %lx\n",page_to_out->va);
            page_to_out->in_disk = 0;
            uintptr_t *pte = page_to_out->pte;
            //V位拉高
            set_attribute(pte, _PAGE_PRESENT);
            pcb->p_num++;

            //把节点放到最后
            list_del(node);
            list_add(node, &(pcb->page_list));
            return 1;
        }

        node = node->next;
    }
    return 0;
}



void freePage(ptr_t baseAddr)
{
    // TODO [P4-task1] (design you 'freePage' here if you need):
    page_t *page = (page_t *)kmalloc(sizeof(page_t));
    page->pa = baseAddr;
    list_add(&(page->list), &free_page_list);
}

static uintptr_t free_head = 0;
static uintptr_t free_tail = 0;
void *kmalloc(size_t size)
{
    // TODO [P4-task1] (design you 'kmalloc' here if you need):

    size += (16 - size%16);

    if(free_head + size > free_tail)
    {
        int page_num = (size + PAGE_SIZE - 1) / PAGE_SIZE;
        free_head = allocPage(page_num);
        free_tail = free_head + page_num*PAGE_SIZE;
    }

    uintptr_t ret = free_head;
    free_head += size;
    return (void *)ret;
}


/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    // TODO [P4-task1] share_pgtable:
    // PTE *src = (PTE *)src_pgdir;
    // PTE *dest = (PTE *)dest_pgdir;

    memcpy((uint8_t*)dest_pgdir,
            (uint8_t*)src_pgdir,
            PAGE_SIZE);

    /*
    uintptr_t kva = pa2kva(PGDIR_PA);
    kva &= VA_MASK;
    uint64_t vpn2 = kva >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);

    dest[vpn2] = src[vpn2];*/
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
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

uintptr_t* get_pte(uintptr_t va, uintptr_t pgdir)
{
    va = va & VA_MASK;
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) & ~(~0 << PPN_BITS);
    uint64_t vpn1 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & ~(~0 << PPN_BITS);
    uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + 2*PPN_BITS)) & ~(~0 << PPN_BITS);

    PTE *pte0 = NULL;
    PTE *pte1 = NULL;
    PTE *pte2 = (PTE *)pgdir + vpn2;

    if(((*pte2) & 0x1) == 0){
        return 0;
    }
    pte1 = (PTE*)pa2kva(((*pte2) >> 10) << 12) + vpn1;

    if(((*pte1) & 0x1) == 0){
        return 0;
    }
    pte0 = (PTE*)pa2kva(((*pte1) >> 10) << 12) + vpn0;

    if(((*pte0) & 0x1) == 0){
        return 0;
    }

    return pte0;
}

uintptr_t shm_page_get(int key)
{
    // TODO [P4-task4] shm_page_get:
    uint64_t cpu_id = get_current_cpu_id();
    int shm_id = key % MAX_SHM_NUM;

    uintptr_t va = SHARE_U_MEM_BASE + current_running[cpu_id]->share_p_num*PAGE_SIZE;
    uintptr_t pa = SHARE_P_MEM_BASE + shm_id*PAGE_SIZE;
    ptr_t pgdir = pa2kva(current_running[cpu_id]->pgdir);

    current_running[cpu_id]->share_p_num++;
    share[shm_id].va[share[shm_id].num] = va;
    share[shm_id].num++;

    uint64_t va_V39 = va & VA_MASK;
    uint64_t vpn0 = (va_V39 >> NORMAL_PAGE_SHIFT) & ~(~0 << PPN_BITS);
    uint64_t vpn1 = (va_V39 >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & ~(~0 << PPN_BITS);
    uint64_t vpn2 = (va_V39 >> (NORMAL_PAGE_SHIFT + 2*PPN_BITS)) & ~(~0 << PPN_BITS);

    PTE *pte0 = NULL;
    PTE *pte1 = NULL;
    PTE *pte2 = (PTE *)pgdir + vpn2;

    if(get_attribute(*pte2, _PAGE_PRESENT) == 0)
    {
        uint64_t pt1_va = allocPage(1);
        uint64_t pt1_pa = kva2pa(pt1_va);
        *pte2 = (pt1_pa >> NORMAL_PAGE_SHIFT) << _PAGE_PFN_SHIFT;
        set_attribute(pte2, _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(*pte2)));
        pte1 = (PTE *)pt1_va + vpn1;
    }
    else 
    {
        uintptr_t pt1_pa = (*pte2 >> _PAGE_PFN_SHIFT) << NORMAL_PAGE_SHIFT;
        uintptr_t pt1_va = pa2kva(pt1_pa);

        pte1 = (PTE *)pt1_va + vpn1;
    }

    if(get_attribute(*pte1, _PAGE_PRESENT) == 0)
    {
        uint64_t pt0_va = allocPage(1);
        uint64_t pt0_pa = kva2pa(pt0_va);
        *pte1 = (pt0_pa >> NORMAL_PAGE_SHIFT) << _PAGE_PFN_SHIFT;
        set_attribute(pte1, _PAGE_PRESENT);
        clear_pgdir(pa2kva(get_pa(*pte1)));
        pte0 = (PTE *)pt0_va + vpn0;
    }
    else 
    {
        uintptr_t pt0_pa = (*pte1 >> _PAGE_PFN_SHIFT) << NORMAL_PAGE_SHIFT;
        uintptr_t pt0_va = pa2kva(pt0_pa);

        pte0 = (PTE *)pt0_va + vpn0;
    }

    if(get_attribute(*pte0, _PAGE_PRESENT) != 0)
        return 0;  //已经映射过了


    uintptr_t return_pa = pa;
    set_pfn(pte0, return_pa >> NORMAL_PAGE_SHIFT);
    set_attribute(pte0, _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC |
                  _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY);

    if(share[shm_id].num == 1 && share[shm_id].used == 0)
    {
        uintptr_t kva = pa2kva(pa);
        for(int i = 0; i < PAGE_SIZE; i++)
        {
            *((char*)kva + i) = 0;
        }
        share[shm_id].used = 1;
    }

    return va;
}

void shm_page_dt(uintptr_t addr)
{
    // TODO [P4-task4] shm_page_dt:
    uint64_t cpu_id = get_current_cpu_id();
    int shm_id = 0;
    int mark = 0;
    int j = 0;

    for(int i = 0; i < MAX_SHM_NUM; i++)
    {
        for(j = 0; j < 16; j++)
        {
            if(share[i].va[j] == addr)
            {
                shm_id = i;
                mark = 1;
                break;
            }
        }
        if(mark)
            break;
    }

    //取消addr的映射
    PTE *pte = (PTE *)get_pte(addr, pa2kva(current_running[cpu_id]->pgdir));
    //set_attribute(pte, 0);
    *pte = 0;

    //处理数组的内容
    share[shm_id].num--;
    share[shm_id].va[j] = 0;

    if(share[shm_id].num == 0)
        share[shm_id].used = 0;
}

void copy_on_write(PTE* son_pgdir, PTE *par_pgdir)
{
    for(int i = 0; i < 512; i++)
    {
        if(get_attribute(par_pgdir[i], _PAGE_PRESENT) != 0)
        {
            PTE *son_pt1 = (PTE *)allocPage(1);
            ptr_t pt1_pa = kva2pa((uintptr_t)son_pt1);
            //set_pfn_s(&son_pgdir[i],kva2pa((uintptr_t)son_pt1)>>NORMAL_PAGE_SHIFT);
            son_pgdir[i] = (pt1_pa >> NORMAL_PAGE_SHIFT) << _PAGE_PFN_SHIFT;
            set_attribute(&son_pgdir[i], _PAGE_PRESENT);
            //PTE *par_pt1 = (PTE *)pa2kva(get_pa_s(par_pgdir[i]));
            PTE *par_pt1 = son_pt1 + i;

            for(uint64_t j = 0; j < 512; j++)
            {
                if(get_attribute(par_pt1[j], _PAGE_PRESENT) != 0)
                {
                    PTE *son_pt0 = (PTE *)allocPage(1);
                    ptr_t pt0_pa = kva2pa((uintptr_t)son_pt0);
                    //set_pfn_s(&son_pt1[j], kva2pa((uintptr_t)son_pt0)>>NORMAL_PAGE_SHIFT);
                    son_pt1[j] = (pt0_pa >> NORMAL_PAGE_SHIFT) << _PAGE_PFN_SHIFT;
                    set_attribute(&son_pt1[j], _PAGE_PRESENT);
                    //PTE *par_pt0 = (PTE *)pa2kva(get_pa_s(par_pt1[j]));
                    PTE *par_pt0 = son_pt0 + j;

                    for(uint64_t k = 0; k < 512; k++)
                    {
                        if(get_attribute(par_pt0[k], _PAGE_PRESENT) != 0)
                        {
                            memcpy((uint8_t *)&son_pt0[k],
                            (uint8_t *)&par_pt0[k],
                            sizeof(PTE));
                            //set_pfn(&son_pt0[k], kva2pa(allocPage(1))>>NORMAL_PAGE_SHIFT);
                            uint64_t mark = get_attribute(son_pt0[k], ~_PAGE_WRITE);
                            set_attribute(&son_pt0[k], mark);

                        }
                        else 
                            continue; 
                    }
                }
                else
                    continue;
            }
        }
        else
            continue;
    }
}