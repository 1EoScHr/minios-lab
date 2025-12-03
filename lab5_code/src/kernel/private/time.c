#include <arch/x86.h>
#include <assert.h>
#include <interrupt.h>
#include <process.h>
#include <schedule.h>
#include <stddef.h>
#include <time.h>
#include <stdio.h>

static size_t SYSTEM_TICK;

void init_sysclk() {
  //! use 8253 PIT timer0 as system clock
  outb(TIMER_MODE, RATE_GENERATOR);
  outb(TIMER0, (uint8_t)((TIMER_FREQ / SYSCLK_FREQ_HZ) >> 0));
  outb(TIMER0, (uint8_t)((TIMER_FREQ / SYSCLK_FREQ_HZ) >> 8));
  SYSTEM_TICK = 0;

  //! enable clock irq for 8259A
  put_irq_handler(CLOCK_IRQ, clock_handler);
  enable_irq(CLOCK_IRQ);
}

static void sys_tick_incr() { SYSTEM_TICK++; }

void clock_handler(int irq) {
  sys_tick_incr();
  //kprintf("tick: %u\n", SYSTEM_TICK);

  //for (size_t i = 0; i < NR_PCBS; i ++)
  //{
  //  if (pcb_table[i].pcb.sleep_ticks > 0) pcb_table[i].pcb.sleep_ticks --;
  //}

  if (--p_proc_ready->pcb.ticks == 0) {
    p_proc_ready->pcb.ticks = p_proc_ready->pcb.priority;
    schedule();
  }
}

size_t get_ticks() { return SYSTEM_TICK; }

ssize_t delay_ticks(size_t ticks) {
  //kprintf("switch to pid%d\n", p_proc_ready->pcb.pid);

  // unimplemented();
  /*
  ssize_t ticks_start = SYSTEM_TICK;
  p_proc_ready->pcb.sleep_ticks = ticks - 2;  // 开始睡眠，提前2ticks睡醒，能更好的控制误差
  schedule(); // 主动放弃当前cpu
  */

  ssize_t ticks_start = SYSTEM_TICK, ticks_total;
  while (1)
  {
    ticks_total = SYSTEM_TICK - ticks_start;
    if (ticks - ticks_total <= 2) break;
    schedule();
  }

  // 醒来后，返回实际睡眠时间
  return ticks_total - ticks;
}
