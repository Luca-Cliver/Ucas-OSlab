#include <asm.h>
#include <csr.h>

ENTRY(setup_exception)

  /* TODO: [p2-task3] save exception_handler_entry into STVEC */
  la t0, exception_handler_entry
  csrw CSR_STVEC, t0

  /* TODO: [p2-task4] enable interrupts globally */

  li t0, SR_SIE
  csrw CSR_SSTATUS, t0

  not t0, x0
  csrs CSR_SIE, t0

  ret
ENDPROC(setup_exception)
