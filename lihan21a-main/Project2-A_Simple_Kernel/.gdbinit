set confirm off
set architecture riscv:rv64
set disassemble-next-line auto
set riscv use-compressed-breakpoints yes
set output-radix 16
add-symbol-file build/print1
add-symbol-file build/print2
add-symbol-file build/lock1
add-symbol-file build/lock2
add-symbol-file build/timer
add-symbol-file build/sleep
add-symbol-file build/tcb
add-symbol-file build/fly
