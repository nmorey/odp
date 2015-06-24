/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP timer service
 *
 */

/* For snprint, POSIX timers and sigevent */

#include <odp_buffer_inlines.h>
#include <odp_debug_internal.h>
#include <odp/time.h>
#include <odp_timer_types_internal.h>


/******************************************************************************
 * POSIX timer support
 * Functions that use Linux/POSIX per-process timers and related facilities
 *****************************************************************************/


void _odp_timer_init(odp_timer_pool *tp ODP_UNUSED)
{
	return;
}

void _odp_timer_fini(odp_timer_pool *tp ODP_UNUSED)
{
	return;
}
