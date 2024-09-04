/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                                   Memory Management
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
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
#ifndef MM_H
#define MM_H

#include "os/list.h"
#include "os/sched.h"
#include <type.h>
#include <pgtable.h>

#define MAP_KERNEL 1
#define MAP_USER 2
#define MEM_SIZE 32
#define PAGE_SIZE 4096 // 4K
#define INIT_KERNEL_STACK 0xffffffc052000000
//#define INIT_KERNEL_STACK 0x50500000
#define INIT_KERNEL_STACK_SLAVE 0x50501000
#define INIT_USER_STACK 0x52500000
#define FREEMEM_KERNEL (INIT_KERNEL_STACK+PAGE_SIZE)

/* Rounding; only works for n = power of two */
#define ROUND(a, n)     (((((uint64_t)(a))+(n)-1)) & ~((n)-1))
#define ROUNDDOWN(a, n) (((uint64_t)(a)) & ~((n)-1))

extern ptr_t allocPage(int numPage);
// TODO [P4-task1] */
void freePage(ptr_t baseAddr);

// #define S_CORE
// NOTE: only need for S-core to alloc 2MB large page
#ifdef S_CORE
#define LARGE_PAGE_FREEMEM 0xffffffc056000000
#define USER_STACK_ADDR 0x400000
extern ptr_t allocLargePage(int numPage);
#else
// NOTE: A/C-core
#define USER_STACK_ADDR 0xf00010000
#endif

// TODO [P4-task1] */
extern void init_page();
extern void* kmalloc(size_t size);
extern void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir);
extern uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir);

// TODO [P4-task4]: shm_page_get/dt */
uintptr_t shm_page_get(int key);
void shm_page_dt(uintptr_t addr);

//[p4 t3]
int swap_in(pcb_t *owner_pcb);
int swap_out(pcb_t *pcb, uintptr_t va);
uintptr_t* get_pte(uintptr_t va, uintptr_t pgdir);
uint64_t* get_pte_of(uintptr_t va, uintptr_t pgdir_va);

void copy_on_write(PTE* son_pgdir, PTE *par_pgdir);

typedef struct page{
    uintptr_t pa;
    uintptr_t va;
    list_node_t list;
    uint64_t *pte;
    int in_disk;
    int block_id;
    pid_t owner;
}page_t;
extern list_head free_page_list;
#define LIST2PAGE(listptr) ((page_t *)((void *)(listptr)-STRUCT_OFFSET(page, list)))

typedef struct share
{
    int num;
    bool used;
    uintptr_t va[16];
}share_t;

#endif /* MM_H */

#define _PAGE_RESERVED_SHIFT 54U
#define _PAGE_PPN_MASK ((~0UL << _PAGE_PFN_SHIFT) & ~(~0UL << _PAGE_RESERVED_SHIFT))
static inline uintptr_t get_pa_s(PTE entry)
{
	/* TODO: [P4-task1] */
	return (entry & _PAGE_PPN_MASK) <<
		(NORMAL_PAGE_SHIFT - _PAGE_PFN_SHIFT);
}

static inline void set_pfn_s(PTE *pentry, uint64_t pfn)
{
	/* TODO: [P4-task1] */
	*pentry = (*pentry & ~_PAGE_PPN_MASK) |
		((pfn << _PAGE_PFN_SHIFT) & _PAGE_PPN_MASK);
}