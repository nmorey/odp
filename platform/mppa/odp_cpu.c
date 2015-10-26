/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp/cpu.h>
#include <odp/hints.h>
#include <odp_cpu_internal.h>
#include <HAL/hal/hal.h>

uint64_t odp_cpu_cycles(void)
{
	return __k1_read_dsu_timestamp();
}

uint64_t odp_cpu_cycles_diff(uint64_t c1, uint64_t c2)
{
	return _odp_cpu_cycles_diff(c1, c2);
}

uint64_t odp_cpu_cycles_max(void)
{
	return UINT64_MAX;
}

uint64_t odp_cpu_cycles_resolution(void)
{
	return 1;
}
