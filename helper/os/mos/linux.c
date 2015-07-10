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
#include <utask.h>

#include <odp/helper/linux.h>
#include <odp/thread.h>
#include <odp/init.h>
#include <odp/system_info.h>
#include "odph_debug.h"

static void *odp_run_start_routine(void *arg)
{
	odp_start_args_t *start_args = arg;
	/* ODP thread local init */
	if (odp_init_local()) {
		ODPH_ERR("Local init failed\n");
		return NULL;
	}

	void *ret_ptr = start_args->start_routine(start_args->arg);
	int ret = odp_term_local();
	if (ret < 0)
		ODPH_ERR("Local term failed\n");
	else if (ret == 0 && odp_term_global())
		ODPH_ERR("Global term failed\n");

	return ret_ptr;
}

int odph_linux_pthread_create(odph_linux_pthread_t *thread_tbl,
			       const odp_cpumask_t *mask_in,
			       void *(*start_routine) (void *), void *arg)
{
	int i;
	int num;
	odp_cpumask_t mask;
	int cpu_count;
	int cpu;

	odp_cpumask_copy(&mask, mask_in);
	num = odp_cpumask_count(&mask);

	memset(thread_tbl, 0, num * sizeof(odph_linux_pthread_t));

	cpu_count = odp_cpu_count();

	if (num < 1 || num > cpu_count) {
		ODPH_ERR("Bad num\n");
		return 0;
	}

	cpu = odp_cpumask_first(&mask);
	for (i = 0; i < num; i++) {
		odp_cpumask_t thd_mask;

		if (cpu == 0  || cpu > cpu_count) {
			ODPH_ERR("Bad cpu\n");
			return i;
		}

		odp_cpumask_zero(&thd_mask);
		odp_cpumask_set(&thd_mask, cpu);

		thread_tbl[i].cpu = cpu;
		thread_tbl[i].start_args = malloc(sizeof(odp_start_args_t));
		if (thread_tbl[i].start_args == NULL)
			ODPH_ABORT("Malloc failed");

		thread_tbl[i].start_args->start_routine = start_routine;
		thread_tbl[i].start_args->arg           = arg;
		utask_t task;
		if(utask_start_pe(&task, odp_run_start_routine, thread_tbl[i].start_args, cpu))
			ODPH_ABORT("Thread failed");
		thread_tbl[i].thread = task.val;
		cpu = odp_cpumask_next(&mask, cpu);
	}
	return i;
}

void odph_linux_pthread_join(odph_linux_pthread_t *thread_tbl, int num)
{
	int i;

	for (i = 0; i < num; i++) {
		/* Wait thread to exit */
		utask_t task;
		task.val = thread_tbl[i].thread;
		utask_join(task, NULL);
		free(thread_tbl[i].start_args);
	}

}

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
