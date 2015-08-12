#ifndef __HAL_DELAY_H__
#define __HAL_DELAY_H__
#ifdef DELAY_DEBUG
#include <stdio.h>
#endif

static inline void mppa_cycle_delay(unsigned int cycle_delay_nb)
{
  unsigned int cnt = cycle_delay_nb >> 2;

#ifdef DELAY_DEBUG
  printf("[DELAY DEBUG] Call mppa_cycle_delay(%u).\n", cycle_delay_nb);
#endif
  // Following ASM code exactly consume 4 clk cycles per instruction.
  __asm__ volatile (
		    "1:\n"
		    "nop\n"
		    ";;\n"
		    "cb.nez %0, 1b\n"
		    "add %0 = %0, -1\n"
		    ";;\n"
		    : "=r" (cnt)
		    : "0" (cnt)
		    :
		    );
#ifdef DELAY_DEBUG
  printf("[DELAY DEBUG] Return from mppa_cycle_delay(%u).\n", cycle_delay_nb);
#endif
}

#endif
