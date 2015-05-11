/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP buffer pool - internal header
 */

#ifndef ODP_POOL_INTERNAL_H_
#define ODP_POOL_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/std_types.h>
#include <odp/align.h>
#include <odp_align_internal.h>
#include <odp/pool.h>
#include <odp_buffer_internal.h>
#include <odp/hints.h>
#include <odp/config.h>
#include <odp/debug.h>
#include <odp/shared_memory.h>
#include <odp/atomic.h>
#include <odp_atomic_internal.h>
#include <string.h>

/**
 * Buffer initialization routine prototype
 *
 * @note Routines of this type MAY be passed as part of the
 * _odp_buffer_pool_init_t structure to be called whenever a
 * buffer is allocated to initialize the user metadata
 * associated with that buffer.
 */
typedef void (_odp_buf_init_t)(odp_buffer_t buf, void *buf_init_arg);

/**
 * Buffer pool initialization parameters
 * Used to communicate buffer pool initialization options. Internal for now.
 */
typedef struct _odp_buffer_pool_init_t {
	size_t udata_size;         /**< Size of user metadata for each buffer */
	_odp_buf_init_t *buf_init; /**< Buffer initialization routine to use */
	void *buf_init_arg;        /**< Argument to be passed to buf_init() */
} _odp_buffer_pool_init_t;         /**< Type of buffer initialization struct */

/* Use ticketlock instead of spinlock */
#define POOL_USE_TICKETLOCK

#ifdef POOL_USE_TICKETLOCK
#include <odp/ticketlock.h>
#define POOL_LOCK(a)      odp_ticketlock_lock(a)
#define POOL_UNLOCK(a)    odp_ticketlock_unlock(a)
#define POOL_LOCK_INIT(a) odp_ticketlock_init(a)
#else
#include <odp/spinlock.h>
#define POOL_LOCK(a)      odp_spinlock_lock(a)
#define POOL_UNLOCK(a)    odp_spinlock_unlock(a)
#define POOL_LOCK_INIT(a) odp_spinlock_init(a)
#endif

struct pool_entry_s {
#ifdef POOL_USE_TICKETLOCK
	odp_ticketlock_t        lock ODP_ALIGNED_CACHE;
	odp_ticketlock_t        buf_lock;
	odp_ticketlock_t        blk_lock;
#else
	odp_spinlock_t          lock ODP_ALIGNED_CACHE;
	odp_spinlock_t          buf_lock;
	odp_spinlock_t          blk_lock;
#endif

	char                    name[ODP_POOL_NAME_LEN];
	odp_pool_param_t        params;
	_odp_buffer_pool_init_t init_params;
	odp_pool_t              pool_hdl;
	uint32_t                pool_id;
	odp_shm_t               pool_shm;
	union {
		uint32_t all;
		struct {
			uint32_t has_name:1;
			uint32_t user_supplied_shm:1;
			uint32_t unsegmented:1;
			uint32_t zeroized:1;
			uint32_t predefined:1;
		};
	} flags;
	uint32_t                quiesced;
	uint32_t                low_wm_assert;
	uint8_t                *pool_base_addr;
	uint8_t                *pool_mdata_addr;
	size_t                  pool_size;
	uint32_t                buf_align;
	uint32_t                buf_stride;
	odp_buffer_hdr_t       *buf_freelist;
	void                   *blk_freelist;
	odp_atomic_u32_t        bufcount;
#ifdef POOL_STATS
	odp_atomic_u32_t        blkcount;
	odp_atomic_u64_t        bufallocs;
	odp_atomic_u64_t        buffrees;
	odp_atomic_u64_t        blkallocs;
	odp_atomic_u64_t        blkfrees;
	odp_atomic_u64_t        bufempty;
	odp_atomic_u64_t        blkempty;
#endif
	uint32_t                buf_num;
	uint32_t                seg_size;
	uint32_t                blk_size;
	uint32_t                headroom;
	uint32_t                tailroom;
};

typedef union pool_entry_u {
	struct pool_entry_s s;

	uint8_t pad[ODP_CACHE_LINE_SIZE_ROUNDUP(sizeof(struct pool_entry_s))];
} pool_entry_t;

extern void *pool_entry_ptr[];

#if defined(ODP_CONFIG_SECURE_POOLS) && (ODP_CONFIG_SECURE_POOLS == 1)
#define buffer_is_secure(buf) (buf->flags.zeroized)
#define pool_is_secure(pool) (pool->flags.zeroized)
#else
#define buffer_is_secure(buf) 0
#define pool_is_secure(pool) 0
#endif

static inline void *get_blk(struct pool_entry_s *pool)
{
	odp_buf_blk_t *myhead;
	POOL_LOCK(&pool->blk_lock);

	myhead = LOAD_PTR(pool->blk_freelist);

	if (odp_unlikely(myhead == NULL)) {
		POOL_UNLOCK(&pool->blk_lock);
#ifdef POOL_STATS
		odp_atomic_inc_u64(&pool->blkempty);
#endif
	} else {
		INVALIDATE(myhead);
		STORE_PTR(pool->blk_freelist, myhead->next);
		POOL_UNLOCK(&pool->blk_lock);
#ifdef POOL_STATS
		odp_atomic_dec_u32(&pool->blkcount);
#endif
	}

	return myhead;
}

static inline void ret_blk(struct pool_entry_s *pool, void *block)
{
	POOL_LOCK(&pool->blk_lock);

	STORE_PTR(((odp_buf_blk_t *)block)->next, LOAD_PTR(pool->blk_freelist));
	STORE_PTR(pool->blk_freelist, block);

	POOL_UNLOCK(&pool->blk_lock);

#ifdef POOL_STATS
	odp_atomic_inc_u32(&pool->blkcount);
	odp_atomic_inc_u64(&pool->blkfrees);
#endif
}

static inline odp_buffer_hdr_t *get_buf(struct pool_entry_s *pool)
{
	odp_buffer_hdr_t *myhead;
	POOL_LOCK(&pool->buf_lock);

	myhead = LOAD_PTR(pool->buf_freelist);

	if (odp_unlikely(myhead == NULL)) {
		POOL_UNLOCK(&pool->buf_lock);
#ifdef POOL_STATS
		odp_atomic_inc_u64(&pool->bufempty);
#endif
	} else {
		INVALIDATE(myhead);
		STORE_PTR(pool->buf_freelist, myhead->next);
		uint32_t bufcount = LOAD_U32(pool->bufcount);
		STORE_U32(pool->bufcount, bufcount - 1);
		POOL_UNLOCK(&pool->buf_lock);

#ifdef POOL_STATS
		odp_atomic_inc_u64(&pool->bufallocs);
#endif
		myhead->allocator = odp_thread_id();
	}

	return (void *)myhead;
}

static inline void ret_buf(struct pool_entry_s *pool, odp_buffer_hdr_t *buf)
{
	if (!buf->flags.hdrdata && buf->type != ODP_EVENT_BUFFER) {
		while (buf->segcount > 0) {
			if (buffer_is_secure(buf) || pool_is_secure(pool))
				memset(buf->addr[buf->segcount - 1],
				       0, buf->segsize);
			ret_blk(pool, buf->addr[--buf->segcount]);
		}
		buf->size = 0;
	}

	buf->allocator = ODP_FREEBUF;  /* Mark buffer free */
	POOL_LOCK(&pool->buf_lock);

	buf->next = LOAD_PTR(pool->buf_freelist);
	STORE_PTR(pool->buf_freelist, buf);

	uint32_t bufcount = LOAD_U32(pool->bufcount) + 1;
	STORE_U32(pool->bufcount, bufcount);
	POOL_UNLOCK(&pool->buf_lock);

#ifdef POOL_STATS
	odp_atomic_inc_u64(&pool->buffrees);
#endif
}

static inline odp_pool_t pool_index_to_handle(uint32_t pool_id)
{
	return _odp_cast_scalar(odp_pool_t, pool_id);
}

static inline uint32_t pool_handle_to_index(odp_pool_t pool_hdl)
{
	return _odp_typeval(pool_hdl);
}

static inline void *get_pool_entry(uint32_t pool_id)
{
	return pool_entry_ptr[pool_id];
}

static inline pool_entry_t *odp_pool_to_entry(odp_pool_t pool)
{
	return (pool_entry_t *)get_pool_entry(pool_handle_to_index(pool));
}

static inline pool_entry_t *odp_buf_to_pool(odp_buffer_hdr_t *buf)
{
	return odp_pool_to_entry(buf->pool_hdl);
}

static inline uint32_t odp_buffer_pool_segment_size(odp_pool_t pool)
{
	return odp_pool_to_entry(pool)->s.seg_size;
}

static inline uint32_t odp_buffer_pool_headroom(odp_pool_t pool)
{
	return odp_pool_to_entry(pool)->s.headroom;
}

static inline uint32_t odp_buffer_pool_tailroom(odp_pool_t pool)
{
	return odp_pool_to_entry(pool)->s.tailroom;
}

#ifdef __cplusplus
}
#endif

#endif
