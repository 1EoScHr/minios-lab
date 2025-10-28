#include <arch/x86.h>
#include <interrupt.h>
#include <process.h>
#include <stdio.h>
#include <time.h>

static size_t SYSTEM_TICK;

// 时钟中断初始化
void init_sysclk() {
  //! use 8253 PIT timer0 as system clock
  outb(TIMER_MODE, RATE_GENERATOR);
  //outb(TIMER0, (uint8_t)((TIMER_FREQ / 10) >> 0));
  //outb(TIMER0, (uint8_t)((TIMER_FREQ / 10) >> 8));
  outb(TIMER0, (uint8_t)((TIMER_FREQ / 1000) >> 0));
  outb(TIMER0, (uint8_t)((TIMER_FREQ / 1000) >> 8));

  SYSTEM_TICK = 0;

  //! enable clock irq for 8259A
  put_irq_handler(CLOCK_IRQ, clock_handler);
  enable_irq(CLOCK_IRQ);
}

static void sys_tick_incr() { SYSTEM_TICK++; }

size_t clock() { return SYSTEM_TICK; }

void clock_handler(int irq) {
  //kprintf("i%d.", SYSTEM_TICK);
  //kprintf("#.");
  
  ++p_proc_ready;
  /*
  if((p_proc_ready == pcb_table + 2) && (SYSTEM_TICK % 100 > 3))
  {
    p_proc_ready = pcb_table;
  }
  */


  sys_tick_incr();
  if (p_proc_ready >= pcb_table + NR_PCBS) {
    p_proc_ready = pcb_table;
  }
}
