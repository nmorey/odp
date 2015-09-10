/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP buffer descriptor - implementation internal
 */

#ifndef ODP_BUFFER_INTERNAL_H_
#define ODP_BUFFER_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/std_types.h>
#include <odp/atomic.h>
#include <odp/pool.h>
#include <odp/buffer.h>
#include <odp/debug.h>
#include <odp/align.h>
#include <odp_align_internal.h>
#include <odp/config.h>
#include <odp/byteorder.h>
#include <odp/thread.h>
#include <odp/event.h>
#include <odp_forward_typedefs_internal.h>


#define ODP_BITSIZE(x) \
	((x) <=     2 ?  1 : \
	((x) <=     4 ?  2 : \
	((x) <=     8 ?  3 : \
	((x) <=    16 ?  4 : \
	((x) <=    32 ?  5 : \
	((x) <=    64 ?  6 : \
	((x) <=   128 ?  7 : \
	((x) <=   256 ?  8 : \
	((x) <=   512 ?  9 : \
	((x) <=  1024 ? 10 : \
	((x) <=  2048 ? 11 : \
	((x) <=  4096 ? 12 : \
	((x) <=  8196 ? 13 : \
	((x) <= 16384 ? 14 : \
	((x) <= 32768 ? 15 : \
	((x) <= 65536 ? 16 : \
	 (0/0)))))))))))))))))

#define ODP_BUFFER_MAX_SEG 1

#define ODP_BUFFER_POOL_BITS   ODP_BITSIZE(ODP_CONFIG_POOLS)
#define ODP_BUFFER_SEG_BITS    ODP_BITSIZE(ODP_BUFFER_MAX_SEG)
#define ODP_BUFFER_INDEX_BITS  (32 - ODP_BUFFER_POOL_BITS - ODP_BUFFER_SEG_BITS)
#define ODP_BUFFER_PREFIX_BITS (ODP_BUFFER_POOL_BITS + ODP_BUFFER_INDEX_BITS)
#define ODP_BUFFER_MAX_POOLS   (1 << ODP_BUFFER_POOL_BITS)
#define ODP_BUFFER_MAX_BUFFERS (1 << ODP_BUFFER_INDEX_BITS)

#define ODP_BUFFER_MAX_INDEX     (ODP_BUFFER_MAX_BUFFERS - 2)
#define ODP_BUFFER_INVALID_INDEX (ODP_BUFFER_MAX_BUFFERS - 1)

/* Common buffer header */
struct odp_buffer_hdr_t {
	struct odp_buffer_hdr_t *next;       /* next buf in a list--keep 1st */
	union {                              /* Multi-use secondary link */
		struct odp_buffer_hdr_t *prev;
		struct odp_buffer_hdr_t *link;
	};
	union {
		uint32_t all;
		struct {
			uint32_t zeroized:1; /* Zeroize buf data on free */
			uint32_t hdrdata:1;  /* Data is in buffer hdr */
			uint32_t sustain:1;  /* Sustain order */
		};
	} flags;
	int8_t                   type;       /* buffer type */
	int8_t                   event_type; /* for reuse as event */
	uint32_t                 size;       /* max data size */
	odp_pool_t               pool_hdl;   /* buffer pool handle */
	odp_atomic_u32_t         ref_count;  /* reference count */
	union {
		uint64_t         buf_u64;    /* user u64 */
		void            *buf_ctx;    /* user context */
		const void      *buf_cctx;   /* const alias for ctx */
	};
	void                    *uarea_addr; /* user area address */
	uint32_t                 uarea_size; /* size of user area */
	uint32_t                 segcount;   /* segment count */
	uint32_t                 segsize;    /* segment size */
	void                    *addr;       /* block addrs */
	uint64_t                 order;      /* sequence for ordered queues */
	queue_entry_t           *origin_qe;  /* ordered queue origin */
	union {
		queue_entry_t   *target_qe;  /* ordered queue target */
		uint64_t         sync;       /* for ordered synchronization */
	};
};

typedef struct odp_buffer_hdr_stride {
	uint8_t pad[ODP_CACHE_LINE_SIZE_ROUNDUP(sizeof(odp_buffer_hdr_t))];
} odp_buffer_hdr_stride;

/* Raw buffer header */
typedef struct {
	odp_buffer_hdr_t buf_hdr;    /* common buffer header */
} odp_raw_buffer_hdr_t;

/* Free buffer marker */
#define ODP_FREEBUF -1

/* Forward declarations */
odp_buffer_t buffer_alloc(odp_pool_t pool, size_t size);


/*
 * Buffer type
 *
 * @param buf      Buffer handle
 *
 * @return Buffer type
 */
int _odp_buffer_type(odp_buffer_t buf);

#ifdef __cplusplus
}
#endif

#endif
