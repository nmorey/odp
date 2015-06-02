/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP synchronisation
 */

#ifndef ODP_API_SYNC_H_
#define ODP_API_SYNC_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup odp_synchronizers
 *  @{
 */

/**
 * Synchronise stores
 *
 * Ensures that all CPU store operations that precede the odp_sync_stores()
 * call are globally visible before any store operation that follows it.
 */
static inline void odp_sync_stores(void)
{
}


/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
