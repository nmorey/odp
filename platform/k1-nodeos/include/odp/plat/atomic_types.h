/* Copyright (c) 2015, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP atomic operations
 */

#ifndef ODP_ATOMIC_TYPES_H_
#define ODP_ATOMIC_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/align.h>
#include <HAL/hal/hal.h>

/**
 * @internal
 * Atomic 64-bit unsigned integer
 */
struct odp_atomic_u64_s {
	union {
		struct {
			int lock : 1;
			uint64_t v : 63; /**< Actual storage for the atomic variable */
			/* Some architectures do not support lock-free operations on 64-bit
			 * data types. We use a spin lock to ensure atomicity. */
		};
		uint64_t _type;
		uint64_t _u64;
	};
} ODP_ALIGNED(sizeof(uint64_t)); /* Enforce alignement! */;

/**
 * @internal
 * Atomic 32-bit unsigned integer
 */
struct odp_atomic_u32_s {
	union {
		struct {
			uint32_t v; /**< Actual storage for the atomic variable */
			uint32_t lock; /**< Spin lock (if needed) used to ensure atomic access */
		};
		uint32_t _type;
		uint64_t _u64;
	};
} ODP_ALIGNED(sizeof(uint32_t)); /* Enforce alignement! */;

/**
 * @internal
 * Helper macro for lock-based atomic operations on 64-bit integers
 * @param[in,out] atom Pointer to the 64-bit atomic variable
 * @param expr Expression used update the variable.
 * @return The old value of the variable.
 */
#define ATOMIC_OP(atom, expr)												\
	({														\
		typeof(*(atom)) a;											\
		while ((a._u64 = __k1_atomic_test_and_clear(&(atom)->_u64)) == 0ULL){					\
			__k1_cpu_backoff(10);										\
		}													\
		typeof((atom)->_type) ___old_val = a.v;									\
		expr; /* Perform whatever update is desired */								\
		STORE_U64(atom->_u64, a._u64);										\
		___old_val; /* Return old value */									\
	})

#define INVALIDATE_AREA(p, s) do {									\
		const char *__ptr;									\
		for (__ptr = (char*)(p); __ptr < ((char*)(p)) + (s); __ptr += _K1_DCACHE_LINE_SIZE) {	\
			__k1_dcache_invalidate_line((__k1_uintptr_t) __ptr);				\
		}											\
		__k1_dcache_invalidate_line((__k1_uintptr_t) __ptr);					\
	}while(0)

#define INVALIDATE(p) INVALIDATE_AREA((p), sizeof(*p))

#define LOAD_U32(p) ((uint32_t)__builtin_k1_lwu((void*)(&p)))
#define STORE_U32(p, val) __builtin_k1_swu((void*)&(p), (uint32_t)(val))
#define LOAD_S32(p) ((int32_t)__builtin_k1_lwu((void*)(&p)))
#define STORE_S32(p, val) __builtin_k1_swu((void*)&(p), (int32_t)(val))

#define LOAD_U64(p) ((uint64_t)__builtin_k1_ldu((void*)(&p)))
#define STORE_U64(p, val) __builtin_k1_sdu((void*)&(p), (uint64_t)(val))
#define LOAD_S64(p) ((int64_t)__builtin_k1_ldu((void*)(&p)))
#define STORE_S64(p, val) __builtin_k1_sdu((void*)&(p), (int64_t)(val))

#define LOAD_PTR(p) ((void*)(unsigned long)(LOAD_U32(p)))
#define STORE_PTR(p, val) STORE_U32((p), (unsigned long)(val))
/** @addtogroup odp_synchronizers
 *  @{
 */

typedef struct odp_atomic_u64_s odp_atomic_u64_t;

typedef struct odp_atomic_u32_s odp_atomic_u32_t;

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
