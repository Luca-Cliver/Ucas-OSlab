#include <asm.h>

ENTRY(_start)

    /* TODO: [p1-task3] setup C runtime environment for the user program */

    // clear bss
   la t0, __bss_start      //BSS起始地址
   la t1, __BSS_END__        //BSS终止地址
LOOP_clear:
   sw x0, 0(t0)
   addi t0, t0, 1
   blt t0, t1, LOOP_clear  //s0比s1小,还没有全部清0


    //开栈
    addi sp, sp, -8
    sd ra, 0(sp)

    /* TODO: [p1-task3] enter main function */
    call main

    /* TODO: [p1-task3] finish task and return to the kernel, replace this in p3-task2! */
    //ld ra, 0(sp)
    //addi sp, sp, 8
    //ret

    /************************************************************/
    /* TODO: [P3] call sys_exit */
    call sys_exit

ret_kernel:
    ld ra, 0(sp)
    addi sp, sp, 8
    ret
    /************************************************************/
// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)