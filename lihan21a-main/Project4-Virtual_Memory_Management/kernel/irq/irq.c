#include "os/mm.h"
#include "pgtable.h"
#include <os/irq.h>
#include <os/time.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/kernel.h>
#include <printk.h>
#include <assert.h>
#include <screen.h>
#include <os/smp.h>



handler_t irq_table[IRQC_COUNT];
handler_t exc_table[EXCC_COUNT];

void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task3] & [p2-task4] interrupt handler.
    // call corresponding handler by the value of `scause`
    asm volatile("mv %0, tp":"=r"(current_running[get_current_cpu_id()]));
    handler_t *handler = (scause >> 63)? irq_table : exc_table;
    scause = scause & 0x7FFFFFFFFFFFFFFF;
    handler[scause](regs, stval, scause);
}

void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task4] clock interrupt handler.
    // Note: use bios_set_timer to reset the timer and remember to reschedule
    bios_set_timer(get_ticks() + TIMER_INTERVAL);
    do_scheduler();
}
extern void disable_IRQ_S_SOFT(void);

void handle_ipi(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // it seems that even main core itself will receive ipi sent by itself
    // so we do nothing
    // subcore won't enter execption_handler_entry and will handle ipi in main.c
    printk("Successfully enter handle_ipi (main core)! [%s]\n",current_running[get_current_cpu_id()]->name);
    printk("%s aroused!\n",current_running[1]?current_running[1]->name:"Sub core hasn't");
    disable_IRQ_S_SOFT();
}

void init_exception()
{
    /* TODO: [p2-task3] initialize exc_table */
    /* NOTE: handle_syscall, handle_other, etc.*/
    for (int i = 0; i < EXCC_COUNT; i++)
    {
        exc_table[i] = handle_other;
    }
    exc_table[EXCC_SYSCALL] = handle_syscall;
    exc_table[EXCC_INST_PAGE_FAULT] = handle_page_fault;
    exc_table[EXCC_LOAD_PAGE_FAULT] = handle_page_fault;
    exc_table[EXCC_STORE_PAGE_FAULT] = handle_page_fault;

    

    /* TODO: [p2-task4] initialize irq_table */
    /* NOTE: handle_int, handle_other, etc.*/
    for(int i = 0; i < IRQC_COUNT; i++)
    {
        irq_table[i] = handle_other;
    }
    irq_table[IRQC_S_SOFT]  = handle_ipi;
    irq_table[IRQC_S_TIMER] = handle_irq_timer;
    // irq_table[IRQC_M_TIMER] = (handler_t)handle_irq_timer;

    /* TODO: [p2-task3] set up the entrypoint of exceptions */
    setup_exception();
}

void handle_page_fault(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    uint64_t cpu_id = get_current_cpu_id();
    uint64_t *pte = get_pte(stval, pa2kva(current_running[cpu_id]->pgdir));

    if(pte != 0)
    {
        if(scause == EXCC_INST_PAGE_FAULT || scause == EXCC_LOAD_PAGE_FAULT)
            set_attribute(pte, _PAGE_ACCESSED );
        else 
            set_attribute(pte, _PAGE_ACCESSED | _PAGE_DIRTY );

        local_flush_tlb_all();
        return;
    }
    else
    {
        if(swap_out(current_running[cpu_id], (stval >> NORMAL_PAGE_SHIFT) << NORMAL_PAGE_SHIFT))
        {
            local_flush_tlb_all();
            return;
        }

        if(current_running[cpu_id]->p_num >= 3)
        {
            swap_in(current_running[cpu_id]);
        }
        //取新page，建立映射
        uintptr_t new_page;
        new_page = alloc_page_helper(stval, pa2kva(current_running[cpu_id]->pgdir));

        page_t *node = (page_t *)kmalloc(sizeof(page_t));
        node->in_disk = 0;
        node->pa = kva2pa(new_page);
        node->va = (stval >> NORMAL_PAGE_SHIFT) << NORMAL_PAGE_SHIFT;

        list_add(&(node->list), &(current_running[cpu_id]->page_list));
        current_running[cpu_id]->p_num++;
        local_flush_tlb_all();
        //return;
    }
}

void handle_other(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    char* reg_name[] = {
        "zero "," ra  "," sp  "," gp  "," tp  ",
        " t0  "," t1  "," t2  ","s0/fp"," s1  ",
        " a0  "," a1  "," a2  "," a3  "," a4  ",
        " a5  "," a6  "," a7  "," s2  "," s3  ",
        " s4  "," s5  "," s6  "," s7  "," s8  ",
        " s9  "," s10 "," s11 "," t3  "," t4  ",
        " t5  "," t6  "
    };
    for (int i = 0; i < 32; i += 3) {
        for (int j = 0; j < 3 && i + j < 32; ++j) {
            printk("%s : %016lx ",reg_name[i+j], regs->regs[i+j]);
        }
        printk("\n\r");
    }
    printk("sstatus: 0x%lx sbadaddr: 0x%lx scause: %lu\n\r",
           regs->sstatus, regs->sbadaddr, regs->scause);
    printk("sepc: 0x%lx\n\r", regs->sepc);
    printk("tval: 0x%lx cause: 0x%lx\n", stval, scause);
    assert(0);
}
