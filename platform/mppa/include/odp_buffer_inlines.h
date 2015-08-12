/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/**
 * @file
 *
 * Inline functions for ODP buffer mgmt routines - implementation internal
 */

#ifndef ODP_BUFFER_INLINES_H_
#define ODP_BUFFER_INLINES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp_buffer_internal.h>
#include <odp_pool_internal.h>

static inline odp_buffer_t odp_buffer_encode_handle(odp_buffer_hdr_t *hdr)
{
	return (odp_buffer_t)hdr;
}

static inline odp_buffer_t odp_hdr_to_buf(odp_buffer_hdr_t *hdr)
{
	return (odp_buffer_t)hdr;
}

static inline odp_buffer_hdr_t *odp_buf_to_hdr(odp_buffer_t buf)
{
	return (odp_buffer_hdr_t *)buf;
}

static inline uint32_t odp_buffer_refcount(odp_buffer_hdr_t *buf)
{
	return odp_atomic_load_u32(&buf->ref_count);
}

static inline uint32_t odp_buffer_incr_refcount(odp_buffer_hdr_t *buf,
						uint32_t val)
{
	return odp_atomic_fetch_add_u32(&buf->ref_count, val) + val;
}

static inline uint32_t odp_buffer_decr_refcount(odp_buffer_hdr_t *buf,
						uint32_t val)
{
	uint32_t tmp;

	tmp = odp_atomic_fetch_sub_u32(&buf->ref_count, val);

	if (tmp < val) {
		odp_atomic_fetch_add_u32(&buf->ref_count, val - tmp);
		return 0;
	} else {
		return tmp - val;
	}
}

static inline odp_buffer_hdr_t *validate_buf(odp_buffer_t buf)
{
	odp_buffer_hdr_t *buf_hdr = (odp_buffer_hdr_t *)buf;
	pool_entry_t *pool = odp_pool_to_entry(buf_hdr->pool_hdl);

	/* If pool not created, handle is invalid */
	if (pool->s.pool_shm == ODP_SHM_INVALID)
		return NULL;

	/* Handle is valid, so buffer is valid if it is allocated */
	return buf_hdr;
}

int odp_buffer_snprint(char *str, uint32_t n, odp_buffer_t buf);

static inline void *buffer_map(odp_buffer_hdr_t *buf,
			       uint32_t offset,
			       uint32_t *seglen,
			       uint32_t limit)
{
	int seg_offset = offset;

	if (seglen) {
		uint32_t buf_left = limit - offset;
		*seglen = seg_offset + buf_left <= buf->segsize ?
			buf_left : buf->segsize - seg_offset;
	}

	return (void *)(seg_offset + (uint8_t *)buf->addr);
}

static inline int _odp_buffer_event_type(odp_buffer_t buf)
{
	return odp_buf_to_hdr(buf)->event_type;
}

static inline void _odp_buffer_event_type_set(odp_buffer_t buf, int ev)
{
	odp_buf_to_hdr(buf)->event_type = ev;
}

#ifdef __cplusplus
}
#endif

#endif
