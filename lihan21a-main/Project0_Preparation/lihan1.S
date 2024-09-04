.global main
main:

// TODO: [p0-task1] Add your code here!
	addi t0, x0, 1          //t0逐渐递增   
	addi t1, x0, 101        //t1 == 101
	addi t2, x0, 0          //和

L1:
	add  t2, t2, t0         //t2 = t2 + t0
	addi t0, t0, 1          //t0++
	blt  t0, t1, L1     

	addi t1, x0, 101        //t1 == 101

end:
	j end
ret
