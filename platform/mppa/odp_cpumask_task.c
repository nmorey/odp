/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#include <pthread.h>

#include <odp/cpu.h>
#include <odp/cpumask.h>
#include <odp_debug_internal.h>

int odp_cpumask_default_worker(odp_cpumask_t *mask, int num_in)
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
	if (first_cpu > cpu_count)
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

int odp_cpumask_default_control(odp_cpumask_t *mask, int num ODP_UNUSED)
{
	odp_cpumask_zero(mask);
	/* By default all control threads on CPU 0 */
	odp_cpumask_set(mask, 0);
	return 1;
}
