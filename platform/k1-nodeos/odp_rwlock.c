/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <stdbool.h>
#include <odp/atomic.h>
#include <odp_atomic_internal.h>
#include <odp/rwlock.h>

#include <odp_spin_internal.h>

void odp_rwlock_init(odp_rwlock_t *rwlock)
{
	odp_atomic_init_u32(&rwlock->cnt, 0);
}

void odp_rwlock_read_lock(odp_rwlock_t *rwlock)
{
	uint32_t cnt = -1;
	int  is_locked = 0;

	__builtin_k1_wpurge();
	while (is_locked == 0) {
		/* waiting for read lock */
		if ((int32_t)cnt < 0) {
			odp_spin();
			cnt = _odp_atomic_u32_load_mm(&rwlock->cnt, _ODP_MEMMODEL_RLX);
			continue;
		}
		is_locked = _odp_atomic_u32_cmp_xchg_strong_mm(&rwlock->cnt,
				&cnt,
				cnt + 1,
				_ODP_MEMMODEL_ACQ,
				_ODP_MEMMODEL_RLX);
	}
}

void odp_rwlock_read_unlock(odp_rwlock_t *rwlock)
{
	__k1_wmb();
	_odp_atomic_u32_sub_mm(&rwlock->cnt, 1, _ODP_MEMMODEL_RLS);
}

void odp_rwlock_write_lock(odp_rwlock_t *rwlock)
{
	uint32_t cnt = 1;
	int is_locked = 0;

	__builtin_k1_wpurge();
	while (is_locked == 0) {
		/* lock aquired, wait */
		if (cnt != 0) {
			odp_spin();
			cnt = _odp_atomic_u32_load_mm(&rwlock->cnt, _ODP_MEMMODEL_RLX);
			continue;
		}
		is_locked = _odp_atomic_u32_cmp_xchg_strong_mm(&rwlock->cnt,
				&cnt,
				(uint32_t)-1,
				_ODP_MEMMODEL_ACQ,
				_ODP_MEMMODEL_RLX);
	}
}

void odp_rwlock_write_unlock(odp_rwlock_t *rwlock)
{
	__k1_wmb();
	_odp_atomic_u32_store_mm(&rwlock->cnt, 0, _ODP_MEMMODEL_RLS);
}
