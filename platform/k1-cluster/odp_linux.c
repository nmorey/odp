/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <odp/helper/linux.h>
#include <odp_internal.h>
#include <odp/thread.h>
#include <odp/init.h>
#include <odp/system_info.h>
#include <odp_debug_internal.h>

int odph_linux_process_fork_n(odph_linux_process_t *proc_tbl ODP_UNUSED,
			      const odp_cpumask_t *mask_in ODP_UNUSED)
{
	return -1;
}


int odph_linux_process_fork(odph_linux_process_t *proc ODP_UNUSED, int cpu ODP_UNUSED)
{
	return -1;
}


int odph_linux_process_wait_n(odph_linux_process_t *proc_tbl ODP_UNUSED, int num ODP_UNUSED)
{
	return -1;
}

int odph_linux_cpumask_default(odp_cpumask_t *mask, int num_in)
{
	int i;
	int first_cpu = 1;
	int num = num_in;
	int cpu_count;

	cpu_count = odp_cpu_count();

	/*
	 * If no user supplied number or it's too large, then attempt
	 * to use all CPUs
	 */
	if (0 == num)
		num = cpu_count;
	if (cpu_count < num)
		num = cpu_count;

	/*
	 * Always force "first_cpu" to a valid CPU
	 */
	if (first_cpu >= cpu_count)
		first_cpu = cpu_count - 1;

	/* Build the mask */
	odp_cpumask_zero(mask);
	for (i = 0; i < num; i++) {
		int cpu;
		/* Add one for the module as odp_cpu_count only
		 * returned available CPU (ie [1..cpucount]) */
		cpu = (first_cpu + i) % (cpu_count + 1);
		odp_cpumask_set(mask, cpu);
	}

	return num;
}
