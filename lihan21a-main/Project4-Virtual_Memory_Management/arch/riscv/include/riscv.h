#ifndef __RISCV_H__
#define __RISCV_H__

typedef unsigned long uint64_t;

static inline uint64_t
r_sstatus()
{
  uint64_t x;
  asm volatile("csrr %0, sstatus" : "=r" (x) );
  return x;
}

static inline uint64_t
r_sie()
{
  uint64_t x;
  asm volatile("csrr %0, sie" : "=r" (x) );
  return x;
}

static inline uint64_t
r_sepc()
{
  uint64_t x;
  asm volatile("csrr %0, sepc" : "=r" (x) );
  return x;
}

static inline uint64_t
r_stvec()
{
  uint64_t x;
  asm volatile("csrr %0, stvec" : "=r" (x) );
  return x;
}

#endif