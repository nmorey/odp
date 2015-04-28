/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <odp/system_info.h>
#include <odp_internal.h>
#include <odp_debug_internal.h>
#include <odp/align.h>
#include <odp/cpu.h>
#include <string.h>
#include <stdio.h>

/* sysconf */
#include <unistd.h>



typedef struct {
	const char *cpu_arch_str;
	int (*cpuinfo_parser)(FILE *file, odp_system_info_t *sysinfo);

} odp_compiler_info_t;

/*
 * Report the number of online CPU's
 */
static int sysconf_cpu_count(void)
{
	long ret;

	ret = sysconf(_SC_NPROCESSORS_ONLN);
	if (ret < 0)
		return 0;

	return (int)ret;
}

static int huge_page_size(void)
{
	return ODP_PAGE_SIZE;
}

/*
 * HW specific /proc/cpuinfo file parsing
 */

static int cpuinfo_mppa(FILE *file ODP_UNUSED, odp_system_info_t *sysinfo)
{
	sysinfo->cpu_hz = 400000000ULL;
	return 0;
}


static odp_compiler_info_t compiler_info = {
	.cpu_arch_str = "mppa",
	.cpuinfo_parser = cpuinfo_mppa
};



/*
 * Use sysconf and dummy values in generic case
 */


static int systemcpu(odp_system_info_t *sysinfo)
{
	int ret;

	ret = sysconf_cpu_count();
	if (ret == 0) {
		ODP_ERR("sysconf_cpu_count failed.\n");
		return -1;
	}

	sysinfo->cpu_count = ret;

	sysinfo->huge_page_size = huge_page_size();

	/* Dummy values */
	sysinfo->cpu_hz          = 400000000;
	sysinfo->cache_line_size = _K1_DCACHE_LINE_SIZE;

	strncpy(sysinfo->model_str, "K1B - Bostan", sizeof(sysinfo->model_str));

	return 0;
}


/*
 * System info initialisation
 */
int odp_system_info_init(void)
{
	FILE  *file;

	memset(&odp_global_data.system_info, 0, sizeof(odp_system_info_t));

	odp_global_data.system_info.page_size = ODP_PAGE_SIZE;

	file = fopen("/proc/cpuinfo", "rt");
	if (file == NULL) {
		ODP_ERR("Failed to open /proc/cpuinfo\n");
		return -1;
	}

	compiler_info.cpuinfo_parser(file, &odp_global_data.system_info);

	fclose(file);

	if (systemcpu(&odp_global_data.system_info)) {
		ODP_ERR("systemcpu failed\n");
		return -1;
	}

	return 0;
}

/*
 *************************
 * Public access functions
 *************************
 */
uint64_t odp_sys_cpu_hz(void)
{
	return odp_global_data.system_info.cpu_hz;
}

uint64_t odp_sys_huge_page_size(void)
{
	return odp_global_data.system_info.huge_page_size;
}

uint64_t odp_sys_page_size(void)
{
	return odp_global_data.system_info.page_size;
}

const char *odp_sys_cpu_model_str(void)
{
	return odp_global_data.system_info.model_str;
}

int odp_sys_cache_line_size(void)
{
	return odp_global_data.system_info.cache_line_size;
}

int odp_cpu_count(void)
{
	return odp_global_data.system_info.cpu_count;
}
