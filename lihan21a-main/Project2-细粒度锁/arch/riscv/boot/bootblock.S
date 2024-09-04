#include <asm/biosdef.h>
#include <csr.h>
// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001fc

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50201000

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000

.text
.global main

main:
	// fence on all memory and I/O
	fence
    // a0 is mhartid
	bnez a0, secondary

	// TODO: [p1-task1] call BIOS to print string "It's bootblock!"
	li a7, BIOS_PUTSTR
	la a0, msg
	jal bios_func_entry

	// TODO: [p1-task2] call BIOS to read kernel in SD card
	
	


	li a7, BIOS_SDREAD
	la s0, os_size_loc
	la a0, kernel            //load kernel
	lh a1, 0(s0)                 //load in bloclnum
	li a2, 1                  //从第二块block开始
	jal bios_func_entry


	// TODO: [p1-task4] load task-related arguments and pass them to kernel
	

	// TODO: [p1-task2] jump to kernel to start UCAS-OS
	j kernel

	/************************************************************/
secondary:
	/* TODO [P3-task3]: 
	 * 1. Mask all interrupts
	 * 2. let stvec pointer to kernel_main
	 * 3. enable software interrupt for ipi
	 */
	 fence

	 csrw CSR_SIE, zero   //关掉所有中断

	 la t0, kernel
	 csrw CSR_STVEC, t0   //设置中断处理，跳到kernel

	 csrrs zero, CSR_SIE, SIE_SSIE     //开软中断，核间中断，定时中断，控制器中断

	 li t0, SR_SIE
	 csrs CSR_SSTATUS, t0  //开中断

	#  csrrc zero, CSR_SIE, SIE_SSIE
  	#  jr ra



wait_for_wakeup:
	wfi
	j wait_for_wakeup

	/************************************************************/
// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "It's a bootloader...\n\r"


