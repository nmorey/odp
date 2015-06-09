/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * ODP atomic operations
 */

#ifndef ODP_PLAT_ATOMIC_H_
#define ODP_PLAT_ATOMIC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <odp/align.h>
#include <odp/plat/atomic_types.h>

/** @ingroup odp_synchronizers
 *  @{
 */

static inline uint32_t odp_atomic_load_u32(odp_atomic_u32_t *atom)
{
#if defined(__K1A__)
	odp_atomic_u32_t a;
	while(1){
		a._u64 = LOAD_U64(atom->_u64);
		if(a.lock)
			return a.v;
		__k1_cpu_backoff(10);
	}
#elif defined(__K1B__)
	return LOAD_U32(atom->v);
#endif

}

static inline void odp_atomic_store_u32(odp_atomic_u32_t *atom,
					uint32_t val)
{
#if defined(__K1A__)
	__k1_wmb();
	ATOMIC_OP(atom, a.v = val);
#elif defined(__K1B__)
	return STORE_U32(atom->v, val);
#endif
}

static inline void odp_atomic_init_u32(odp_atomic_u32_t *atom, uint32_t val)
{
#if defined(__K1A__)
	odp_atomic_u32_t a;
	__k1_wmb();

	a.v = val;
	a.lock = 0x1;
	STORE_U64(atom->_u64, a._u64);
#elif defined(__K1B__)
	STORE_U64(atom->_u64, 0ULL);
	STORE_U32(atom->v, val);
#endif
}

static inline uint32_t odp_atomic_fetch_add_u32(odp_atomic_u32_t *atom,
						uint32_t val)
{
#if defined(__K1A__)
	return ATOMIC_OP(atom, a.v += val);
#elif defined(__K1B__)
	unsigned long long val64 = val;
	asm volatile ("afdau 0[%1] = %0\n;;\n" : "+r"(val64) : "r" (&atom->_u64): "memory");
	return (unsigned)val64;
#endif
}

static inline void odp_atomic_add_u32(odp_atomic_u32_t *atom,
				      uint32_t val)
{
#if defined(__K1A__)
	(void) ATOMIC_OP(atom, a.v += val);
#elif defined(__K1B__)
	unsigned long long val64 = val;
	asm volatile ("afdau 0[%1] = %0\n;;\n" : "+r"(val64) : "r" (&atom->_u64): "memory");
	return;
#endif
}

static inline uint32_t odp_atomic_fetch_sub_u32(odp_atomic_u32_t *atom,
						uint32_t val)
{
#if defined(__K1A__)
	return ATOMIC_OP(atom, a.v -= val);
#elif defined(__K1B__)
	long long val64 = -val;
	asm volatile ("afdau 0[%1] = %0\n;;\n" : "+r"(val64) : "r" (&atom->_u64): "memory");
	return (unsigned)val64;
#endif
}

static inline void odp_atomic_sub_u32(odp_atomic_u32_t *atom,
				      uint32_t val)
{
#if defined(__K1A__)
	(void) ATOMIC_OP(atom, a.v -= val);
#elif defined(__K1B__)
	long long val64 = -val;
	asm volatile ("afdau 0[%1] = %0\n;;\n" : "+r"(val64) : "r" (&atom->_u64): "memory");
	return;
#endif
}

static inline uint32_t odp_atomic_fetch_inc_u32(odp_atomic_u32_t *atom)
{
	return odp_atomic_fetch_add_u32(atom, 1);
}

static inline void odp_atomic_inc_u32(odp_atomic_u32_t *atom)
{
	odp_atomic_add_u32(atom, 1);
}

static inline uint32_t odp_atomic_fetch_dec_u32(odp_atomic_u32_t *atom)
{
	return odp_atomic_fetch_sub_u32(atom, 1);
}

static inline void odp_atomic_dec_u32(odp_atomic_u32_t *atom)
{
	odp_atomic_sub_u32(atom, 1);
}

static inline uint64_t odp_atomic_load_u64(odp_atomic_u64_t *atom)
{
#if defined(__K1A__)
	odp_atomic_u64_t a;
	while(1){
		a._u64 = LOAD_U64(atom->_u64);
		if(a.lock)
			return a.v;
		__k1_cpu_backoff(10);
	}

#elif defined(__K1B__)
	return LOAD_U64(atom->v);
#endif
}

static inline void odp_atomic_store_u64(odp_atomic_u64_t *atom,
					uint64_t val)
{
#if defined(__K1A__)
	(void) ATOMIC_OP(atom, a.v = val);

#elif defined(__K1B__)
	return STORE_U64(atom->v, val);
#endif
}

static inline void odp_atomic_init_u64(odp_atomic_u64_t *atom, uint64_t val)
{
#if defined(__K1A__)
	odp_atomic_u64_t a;
	a.lock = 1;
	a.v = val;
	STORE_U64(atom->_u64, a._u64);
#elif defined(__K1B__)
	STORE_U64(atom->v, val);
#endif
}

static inline uint64_t odp_atomic_fetch_add_u64(odp_atomic_u64_t *atom,
						uint64_t val)
{
#if defined(__K1A__)
	return ATOMIC_OP(atom, a.v += val);
#elif defined(__K1B__)
	unsigned long long val64 = val;
	asm volatile ("afdau 0[%1] = %0\n;;\n" : "+r"(val64) : "r" (&atom->_u64): "memory");
	return (unsigned)val64;
#endif
}

static inline void odp_atomic_add_u64(odp_atomic_u64_t *atom, uint64_t val)
{
#if defined(__K1A__)
	(void)ATOMIC_OP(atom, a.v += val);
#elif defined(__K1B__)
	unsigned long long val64 = val;
	asm volatile ("afdau 0[%1] = %0\n;;\n" : "+r"(val64) : "r" (&atom->_u64): "memory");
	return;
#endif
}

static inline uint64_t odp_atomic_fetch_sub_u64(odp_atomic_u64_t *atom,
						uint64_t val)
{
#if defined(__K1A__)
	return ATOMIC_OP(atom, a.v -= val);
#elif defined(__K1B__)
	long long val64 = -val;
	asm volatile ("afdau 0[%1] = %0\n;;\n" : "+r"(val64) : "r" (&atom->_u64): "memory");
	return (unsigned)val64;
#endif
}

static inline void odp_atomic_sub_u64(odp_atomic_u64_t *atom, uint64_t val)
{
#if defined(__K1A__)
	(void)ATOMIC_OP(atom, a.v -= val);
#elif defined(__K1B__)
	long long val64 = -val;
	asm volatile ("afdau 0[%1] = %0\n;;\n" : "+r"(val64) : "r" (&atom->_u64): "memory");
	return;
#endif
}

static inline uint64_t odp_atomic_fetch_inc_u64(odp_atomic_u64_t *atom)
{
	return odp_atomic_fetch_add_u64(atom, 1ULL);
}

static inline void odp_atomic_inc_u64(odp_atomic_u64_t *atom)
{
	return odp_atomic_add_u64(atom, 1ULL);
}

static inline uint64_t odp_atomic_fetch_dec_u64(odp_atomic_u64_t *atom)
{
	return odp_atomic_fetch_sub_u64(atom, 1ULL);
}

static inline void odp_atomic_dec_u64(odp_atomic_u64_t *atom)
{
	odp_atomic_sub_u64(atom, 1ULL);
}

/**
 * @}
 */

#include <odp/api/atomic.h>

#ifdef __cplusplus
}
#endif

#endif
