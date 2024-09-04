.global main
main:

// TODO: [p0-task1] Add your code here!

    addi t0, x0, 1       //待检测数,从1开始
    addi t1, x0, 101     //t1 == 101
    addi t2, x0, 1       //t2作为下面取余的除数

L1:
    addi a0, x0, 1       //a0标志是否是质数,1代表是,0代表不是,初始设为1
    addi t2, t2, 1       //t2++
    nop
    bge  t2, t0, L2      //都没有除尽,为质数
    rem  t4, t0, t2      //t4为余数
    nop
    beq  t4, x0, L3      //余数为0,不是质数
    j L1                 //此时没有除尽,继续进行此轮循环


L2:                      //是质数,后续处理
    nop                 //此处查询结果
    addi t0, t0, 1      //t0++
    addi t2, x0, 1      //t2除数复原
    nop
    blt  t0, t1, L1     //t0在100以内继续下一轮循环
    nop
    bge  t0, t1, end

L3:                     //不是质数,后续处理
    addi a0, x0, 0      //a0 = 0
    nop                 //此处查询结果
    addi t0, t0, 1      //t0++
    addi t2, x0, 1      //t2除数复原
    nop
    blt  t0, t1, L1

end:
	j end
ret