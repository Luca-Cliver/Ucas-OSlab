/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0x50500000

.section ".entry_function","ax"
ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  /* TODO: [p1-task2] clear BSS for flat non-ELF images */
  la s0, __bss_start      //BSS起始地址
  la s1, __BSS_END__        //BSS终止地址
LOOP_clear:
  sw x0, 0(s0)
  addi s0, s0, 1
  blt s0, s1, LOOP_clear  //s0比s1小,还没有全部清0

  /* TODO: [p1-task2] setup C environment */
  li sp, KERNEL_STACK     //设置栈指针
  j main

loop:
  wfi
  j loop

END(_start)
