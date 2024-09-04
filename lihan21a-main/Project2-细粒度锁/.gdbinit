set confirm off
set architecture riscv:rv64
set disassemble-next-line auto
set riscv use-compressed-breakpoints yes
set output-radix 16
add-symbol-file /home/stu/lihan21a/build/shell
add-symbol-file /home/stu/lihan21a/build/waitpid
add-symbol-file /home/stu/lihan21a/build/wait_locks
add-symbol-file /home/stu/lihan21a/build/ready_to_exit
add-symbol-file /home/stu/lihan21a/build/barrier
add-symbol-file /home/stu/lihan21a/build/test_barrier
add-symbol-file /home/stu/lihan21a/build/semaphore
add-symbol-file /home/stu/lihan21a/build/sema_consumer
add-symbol-file /home/stu/lihan21a/build/sema_producer
add-symbol-file /home/stu/lihan21a/build/mbox_server
add-symbol-file /home/stu/lihan21a/build/mbox_client
add-symbol-file /home/stu/lihan21a/build/final_test
add-symbol-file /home/stu/lihan21a/build/fine-grained-lock
add-symbol-file /home/stu/lihan21a/build/fly
