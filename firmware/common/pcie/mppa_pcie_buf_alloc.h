#include <inttypes.h>

#ifndef MPPA_PCIE_BUF_ALLOC_H
#define MPPA_PCIE_BUF_ALLOC_H

#define POOL_MULTI_MAX 64

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#define LOAD_U32(p) ((uint32_t)__builtin_k1_lwu((void*)(&p)))
#define STORE_U32(p, val) __builtin_k1_swu((void*)&(p), (uint32_t)(val))

#define LOAD_PTR(p) ((void*)(unsigned long)(LOAD_U32(p)))
#define STORE_PTR(p, val) STORE_U32((p), (unsigned long)(val))

/**
 * @internal
 * Atomic 64-bit unsigned integer
 */
struct odp_atomic_u64_s {
	union {
		uint64_t v;
		uint64_t _type;
		uint64_t _u64;
	};
} __attribute__((aligned(sizeof(uint64_t)))); /* Enforce alignement! */

/**
 * @internal
 * Atomic 32-bit unsigned integer
 */
struct odp_atomic_u32_s {
	union {
		struct {
			uint32_t v; /**< Actual storage for the atomic variable */
			uint32_t _dummy; /**< Dummy field for force struct to 64b */
		};
		uint32_t _type;
		uint64_t _u64;
	};
} __attribute__((aligned(sizeof(uint32_t)))); /* Enforce alignement! */

/** @addtogroup odp_synchronizers
 *  @{
 */

typedef struct odp_atomic_u64_s odp_atomic_u64_t;

typedef struct odp_atomic_u32_s odp_atomic_u32_t;

static inline uint32_t odp_atomic_load_u32(odp_atomic_u32_t *atom)
{
	return LOAD_U32(atom->v);

}

static inline void odp_atomic_store_u32(odp_atomic_u32_t *atom,
					uint32_t val)
{
	return STORE_U32(atom->v, val);
}

typedef struct mppa_pcie_noc_rx_buf {
	void *buf_addr;
	uint8_t pkt_count;		/* Count of packet in this buffer */
} mppa_pcie_noc_rx_buf_t; 

typedef struct {
	mppa_pcie_noc_rx_buf_t **buf_ptrs;
	uint32_t buf_num;
	odp_atomic_u32_t prod_head;
	odp_atomic_u32_t prod_tail;
	odp_atomic_u32_t cons_head;
	odp_atomic_u32_t cons_tail;
} buffer_ring_t;


static inline void buffer_ring_init(buffer_ring_t *ring, mppa_pcie_noc_rx_buf_t **addr,
					uint32_t buf_num)
{
	ring->buf_ptrs = addr;
	__builtin_k1_swu(&ring->prod_head, 0);
	__builtin_k1_swu(&ring->prod_tail, 0);
	__builtin_k1_swu(&ring->cons_head, 0);
	__builtin_k1_swu(&ring->cons_tail, 0);
	ring->buf_num = buf_num;
}

int buffer_ring_get_multi(buffer_ring_t *ring,
			      mppa_pcie_noc_rx_buf_t *buffers[],
			      unsigned n_buffers, uint32_t *left);
void buffer_ring_push_multi(buffer_ring_t *ring,
				mppa_pcie_noc_rx_buf_t *buffers[],
				unsigned n_buffers, uint32_t *left);

static inline uint32_t odp_buffer_ring_get_count(buffer_ring_t *ring)
{
	uint32_t bufcount = __builtin_k1_lwu(&ring->prod_tail) - __builtin_k1_lwu(&ring->cons_tail);
	if(bufcount > ring->buf_num)
		bufcount += ring->buf_num + 1;

	return bufcount;
}


#endif
