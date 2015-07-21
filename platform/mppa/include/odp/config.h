/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP configuration
 */

#ifndef ODP_PLAT_CONFIG_H_
#define ODP_PLAT_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif


#include <odp/api/config.h>

/** @ingroup odp_compiler_optim
 *  @{
 */

#undef ODP_CONFIG_MAX_THREADS
#define ODP_CONFIG_MAX_THREADS  16

#undef ODP_CONFIG_POOLS
#define ODP_CONFIG_POOLS        16

#undef ODP_CONFIG_PKTIO_ENTRIES
#define ODP_CONFIG_PKTIO_ENTRIES 32

#undef ODP_CONFIG_BUFFER_ALIGN_MIN
#define ODP_CONFIG_BUFFER_ALIGN_MIN 16

#undef ODP_CONFIG_BUFFER_ALIGN_MAX
#define ODP_CONFIG_BUFFER_ALIGN_MAX 64

#undef ODP_CONFIG_PACKET_SEG_LEN_MIN
#define ODP_CONFIG_PACKET_SEG_LEN_MIN (64)

#undef ODP_CONFIG_PACKET_SEG_LEN_MAX
#define ODP_CONFIG_PACKET_SEG_LEN_MAX (9000)

#undef ODP_CONFIG_PACKET_BUF_LEN_MAX
#define ODP_CONFIG_PACKET_BUF_LEN_MAX (ODP_CONFIG_PACKET_SEG_LEN_MAX + \
				       ODP_CONFIG_PACKET_HEADROOM + \
				       ODP_CONFIG_PACKET_TAILROOM)

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
