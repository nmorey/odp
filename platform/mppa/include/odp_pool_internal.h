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

/* Local cache for buffer alloc/free acceleration */
typedef struct local_cache_t {
	odp_buffer_hdr_t *buf_freelist;  /* The local cache */
	uint64_t bufallocs;              /* Local buffer alloc count */
	uint64_t buffrees;               /* Local buffer free count */
} local_cache_t;

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

/**
 * ODP Pool stats - Maintain some useful stats regarding pool utilization
 */
typedef struct {
	odp_atomic_u64_t bufallocs;     /**< Count of successful buf allocs */
	odp_atomic_u64_t buffrees;      /**< Count of successful buf frees */
	odp_atomic_u64_t blkallocs;     /**< Count of successful blk allocs */
	odp_atomic_u64_t blkfrees;      /**< Count of successful blk frees */
	odp_atomic_u64_t bufempty;      /**< Count of unsuccessful buf allocs */
	odp_atomic_u64_t blkempty;      /**< Count of unsuccessful blk allocs */
	odp_atomic_u64_t high_wm_count; /**< Count of high wm conditions */
	odp_atomic_u64_t low_wm_count;  /**< Count of low wm conditions */
} _odp_pool_stats_t;

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
	uint32_t                udata_size;
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
	odp_atomic_u32_t        blkcount;
#ifdef POOL_STATS
	_odp_pool_stats_t       poolstats;
#endif
	uint32_t                buf_num;
	uint32_t                seg_size;
	uint32_t                blk_size;
	uint32_t                high_wm;
	uint32_t                low_wm;
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
	void *myhead;
	POOL_LOCK(&pool->blk_lock);

	myhead = LOAD_PTR(pool->blk_freelist);

	if (odp_unlikely(myhead == NULL)) {
		POOL_UNLOCK(&pool->blk_lock);
#ifdef POOL_STATS
		odp_atomic_inc_u64(&pool->poolstats.blkempty);
#endif
	} else {
		INVALIDATE((odp_buf_blk_t *)myhead);
		STORE_PTR(pool->blk_freelist, ((odp_buf_blk_t *)myhead)->next);
		POOL_UNLOCK(&pool->blk_lock);
#ifdef POOL_STATS
		odp_atomic_dec_u32(&pool->blkcount);
		odp_atomic_inc_u64(&pool->poolstats.blkallocs);
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
	odp_atomic_inc_u64(&pool->poolstats.blkfrees);
#endif
}

static inline odp_buffer_hdr_t *get_buf(struct pool_entry_s *pool)
{
	odp_buffer_hdr_t *myhead, *newhead, *prevhead;

	while(1) {
		myhead = LOAD_PTR(pool->buf_freelist);
		if (odp_unlikely(myhead == NULL)) {
#ifdef POOL_STATS
		odp_atomic_inc_u64(&pool->poolstats.bufempty);
#endif
			return NULL;
		}
		newhead = LOAD_PTR(myhead->next);
		if((unsigned long)newhead & 0x1UL){
			/* Someone is popping this node */
			continue;
		}
		/* Clear the next field of the first elnt */
		prevhead = CAS_PTR(&myhead->next, (unsigned long)newhead | 0x1UL, newhead);

		if(prevhead != newhead){
			/* Someone just logically cut the first elnt. Try again */
			continue;
		}
		/* Now switch the list HEAD with our new HEAD */
		prevhead = CAS_PTR(&pool->buf_freelist, newhead, myhead);
		if(prevhead != myhead){
			/* Someone pushed a new elnt ! Revert */
			STORE_PTR(myhead->next, newhead);
			continue;
		}
		INVALIDATE(myhead);
		break;

	}

	uint64_t bufcount =
		odp_atomic_fetch_sub_u32(&pool->bufcount, 1) - 1;

	/* Check for low watermark condition */
	if (bufcount == pool->low_wm && !LOAD_U32(pool->low_wm_assert)) {
		STORE_U32(pool->low_wm_assert, 1);
#ifdef POOL_STATS
		odp_atomic_inc_u64(&pool->poolstats.low_wm_count);
#endif
	}

#ifdef POOL_STATS
	odp_atomic_inc_u64(&pool->poolstats.bufallocs);
#endif
	myhead->allocator = odp_thread_id();

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
	__builtin_k1_wpurge();

	odp_buffer_hdr_t *myhead, *prevhead;
	myhead = LOAD_PTR(pool->buf_freelist);
	while(1) {
		STORE_PTR(buf->next, myhead);
		prevhead = CAS_PTR(&pool->buf_freelist, buf, myhead);
		if(myhead == prevhead)
			break;
		myhead = prevhead;
	}
	uint64_t bufcount = odp_atomic_fetch_add_u32(&pool->bufcount, 1) + 1;

	/* Check if low watermark condition should be deasserted */
	if (bufcount == pool->high_wm && LOAD_U32(pool->low_wm_assert)) {
		STORE_U32(pool->low_wm_assert, 0);
#ifdef POOL_STATS
		odp_atomic_inc_u64(&pool->poolstats.high_wm_count);
#endif
	}

#ifdef POOL_STATS
	odp_atomic_inc_u64(&pool->poolstats.buffrees);
#endif
}

static inline void *get_local_buf(local_cache_t *buf_cache,
				  struct pool_entry_s *pool,
				  size_t totsize)
{
	odp_buffer_hdr_t *buf = buf_cache->buf_freelist;

	if (odp_likely(buf != NULL)) {
		buf_cache->buf_freelist = buf->next;

		if (odp_unlikely(buf->size < totsize)) {
			intmax_t needed = totsize - buf->size;

			do {
				void *blk = get_blk(pool);
				if (odp_unlikely(blk == NULL)) {
					ret_buf(pool, buf);
#ifdef POOL_STATS
					buf_cache->buffrees--;
#endif
					return NULL;
				}
				buf->addr[buf->segcount++] = blk;
				needed -= pool->seg_size;
			} while (needed > 0);

			buf->size = buf->segcount * pool->seg_size;
		}

#ifdef POOL_STATS
		buf_cache->bufallocs++;
#endif
		buf->allocator = odp_thread_id();  /* Mark buffer allocated */
	}

	return buf;
}

static inline void ret_local_buf(local_cache_t *buf_cache,
				odp_buffer_hdr_t *buf)
{
	buf->allocator = ODP_FREEBUF;
	buf->next = buf_cache->buf_freelist;
	buf_cache->buf_freelist = buf;

#ifdef POOL_STATS
	buf_cache->buffrees++;
#endif
}

static inline void flush_cache(local_cache_t *buf_cache,
			       struct pool_entry_s *pool)
{
	odp_buffer_hdr_t *buf = buf_cache->buf_freelist;
	uint32_t flush_count = 0;

	while (buf != NULL) {
		odp_buffer_hdr_t *next = buf->next;
		ret_buf(pool, buf);
		buf = next;
		flush_count++;
	}

#ifdef POOL_STATS
	odp_atomic_add_u64(&pool->poolstats.bufallocs, buf_cache->bufallocs);
	odp_atomic_add_u64(&pool->poolstats.buffrees,
			   buf_cache->buffrees - flush_count);

	buf_cache->bufallocs = 0;
	buf_cache->buffrees = 0;
#endif

	buf_cache->buf_freelist = NULL;
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
