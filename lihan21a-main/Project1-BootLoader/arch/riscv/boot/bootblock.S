#include <asm/biosdef.h>

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

	// TODO: [p1-task1] call BIOS to print string "It's bootblock!"
	li a7, BIOS_PUTSTR
	la a0, msg
	jal bios_func_entry

	// TODO: [p1-task2] call BIOS to read kernel in SD card
	
	/*task5:读取解压程序*/
	li a7, BIOS_SDREAD
	la s0, os_size_loc
	la a0, 0x52000000
	lh a1, -6(s0)
	li a2, 1
	jal bios_func_entry

	la t0, 0x52000000
	jalr t0


	li a7, BIOS_SDREAD
	la s0, os_size_loc
	la a0, kernel            //load kernel
	lh a1, 0(s0)                 //load in bloclnum
	li a2, 1                  //从第二块block开始
	jal bios_func_entry


	// TODO: [p1-task4] load task-related arguments and pass them to kernel
	li a7, BIOS_SDREAD
	la s0, os_size_loc
	lh t0, 0(s0)
LOOP_task_sector:
	la a0, kernel
	li a1, 1
	li a2, 1
	jal  bios_func_entry
	addi a2, a2, 1
	addi  a0, a0, 512
	addi t0, t0, -1
	bgtz t0, LOOP_task_sector
	

	// TODO: [p1-task2] jump to kernel to start UCAS-OS
	j kernel

	/************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/
// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "It's a bootloader...\n\r"

