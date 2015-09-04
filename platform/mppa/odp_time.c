/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */
#include <odp/time.h>
#include <HAL/hal/hal.h>

uint64_t odp_time_cycles(void)
{
	return __k1_read_dsu_timestamp();
}
