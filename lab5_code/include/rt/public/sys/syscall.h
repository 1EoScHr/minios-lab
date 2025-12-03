#pragma once

enum {
  NR_get_ticks = 0,
  NR_get_pid,
  NR_read,
  NR_write,
  NR_delay_ticks,

  //! total syscalls
  NR_SYSCALLS = NR_delay_ticks + 1,
};
