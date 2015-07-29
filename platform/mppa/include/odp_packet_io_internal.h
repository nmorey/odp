/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */


/**
 * @file
 *
 * ODP packet IO - implementation internal
 */

#ifndef ODP_PACKET_IO_INTERNAL_H_
#define ODP_PACKET_IO_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <odp/spinlock.h>
#include <odp/rwlock.h>
#include <odp_classification_datamodel.h>
#include <odp_align_internal.h>
#include <odp_debug_internal.h>
#include <odp_buffer_inlines.h>

#include <odp/config.h>
#include <odp/hints.h>

#ifndef IF_NAMESIZE
#define IF_NAMESIZE 256
#endif

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

/* Forward declaration */
struct pktio_if_ops;
struct eth_status;

typedef struct {
	int clus_id;			/**< Cluster ID */
	odp_pool_t pool; 		/**< pool to alloc packets from */
	size_t max_frame_len; 		/**< max frame len = buf_size - sizeof(pkt_hdr) */
	size_t buf_size; 		/**< size of buffer payload in 'pool' */
	uint64_t sent_pkt_count;	/**< Count of packet sent to the clusters */
	uint64_t recv_pkt_count;	/**< Count of packet received */
} pkt_cluster_t;

typedef struct {
	int fd;				/**< magic syscall eth interface file descriptor */
	odp_pool_t pool; 		/**< pool to alloc packets from */
	size_t max_frame_len; 		/**< max frame len = buf_size - sizeof(pkt_hdr) */
	size_t buf_size; 		/**< size of buffer payload in 'pool' */
} pkt_magic_t;

typedef struct {
	odp_queue_t loopq;		/**< loopback queue for "loop" device */
	odp_bool_t promisc;		/**< promiscuous mode state */
} pkt_loop_t;

typedef struct {
	struct eth_status * status;          /**< Ethernet internal data */
} pkt_eth_t;

struct pktio_entry {
	const struct pktio_if_ops *ops; /**< Implementation specific methods */
	odp_rwlock_t lock;		/**< entry RW lock */
	int taken;			/**< is entry taken(1) or free(0) */
	int cls_enabled;		/**< is classifier enabled */
	odp_pktio_t handle;		/**< pktio handle */
	odp_queue_t inq_default;	/**< default input queue, if set */
	odp_queue_t outq_default;	/**< default out queue */
	classifier_t cls;		/**< classifier linked with this pktio*/
	char name[IF_NAMESIZE];	        /**< name of pktio provided to
					     pktio_open() */


	union {
		pkt_magic_t pkt_magic;
		pkt_loop_t pkt_loop;
		pkt_cluster_t pkt_cluster;
		pkt_eth_t pkt_eth;
	};
};

typedef union {
	struct pktio_entry s;
	uint8_t pad[ODP_CACHE_LINE_SIZE_ROUNDUP(sizeof(struct pktio_entry))];
} pktio_entry_t;

typedef struct {
	odp_spinlock_t lock;
	pktio_entry_t entries[ODP_CONFIG_PKTIO_ENTRIES];
} pktio_table_t;

typedef struct pktio_if_ops {
	int (*init)(void);
	int (*term)(void);
	int (*open)(odp_pktio_t pktio, pktio_entry_t *pktio_entry,
		    const char *devname, odp_pool_t pool);
	int (*close)(pktio_entry_t *pktio_entry);
	int (*recv)(pktio_entry_t *pktio_entry, odp_packet_t pkt_table[],
		    unsigned len);
	int (*send)(pktio_entry_t *pktio_entry, odp_packet_t pkt_table[],
		    unsigned len);
	int (*mtu_get)(pktio_entry_t *pktio_entry);
	int (*promisc_mode_set)(pktio_entry_t *pktio_entry,  int enable);
	int (*promisc_mode_get)(pktio_entry_t *pktio_entry);
	int (*mac_get)(pktio_entry_t *pktio_entry, void *mac_addr);
} pktio_if_ops_t;

extern void *pktio_entry_ptr[];

static inline int pktio_to_id(odp_pktio_t pktio)
{
	return _odp_typeval(pktio) - 1;
}

static inline pktio_entry_t *get_pktio_entry(odp_pktio_t pktio)
{
	if (odp_unlikely(pktio == ODP_PKTIO_INVALID))
		return NULL;

	if (odp_unlikely(_odp_typeval(pktio) > ODP_CONFIG_PKTIO_ENTRIES)) {
		ODP_DBG("pktio limit %d/%d exceed\n",
			_odp_typeval(pktio), ODP_CONFIG_PKTIO_ENTRIES);
		return NULL;
	}

	return pktio_entry_ptr[pktio_to_id(pktio)];
}

int pktin_poll(pktio_entry_t *entry);

extern const pktio_if_ops_t loopback_pktio_ops;
extern const pktio_if_ops_t magic_pktio_ops;
extern const pktio_if_ops_t cluster_pktio_ops;
extern const pktio_if_ops_t eth_pktio_ops;
extern const pktio_if_ops_t drop_pktio_ops;
extern const pktio_if_ops_t * const pktio_if_ops[];

typedef struct _odp_pkt_iovec {
	void    *iov_base;
	uint32_t iov_len;
} odp_pkt_iovec_t;

static inline
uint32_t _tx_pkt_to_iovec(odp_packet_t pkt,
			  odp_pkt_iovec_t iovecs[ODP_BUFFER_MAX_SEG])
{
	uint32_t pkt_len = odp_packet_len(pkt);
	uint32_t offset = odp_packet_l2_offset(pkt);
	uint32_t iov_count = 0;

	while (offset < pkt_len) {
		uint32_t seglen;

		iovecs[iov_count].iov_base = odp_packet_offset(pkt, offset,
							       &seglen, NULL);
		iovecs[iov_count].iov_len = seglen;
		iov_count++;
		offset += seglen;
	}

	return iov_count;
}

static inline
uint32_t _rx_pkt_to_iovec(odp_packet_t pkt,
			  odp_pkt_iovec_t iovecs[ODP_BUFFER_MAX_SEG])
{
	odp_packet_seg_t seg = odp_packet_first_seg(pkt);
	uint32_t seg_count = odp_packet_num_segs(pkt);
	uint32_t seg_id = 0;
	uint32_t iov_count = 0;
	odp_packet_hdr_t *pkt_hdr = odp_packet_hdr(pkt);
	uint8_t *ptr;
	uint32_t seglen;

	for (seg_id = 0; seg_id < seg_count; ++seg_id) {
		ptr = segment_map(&pkt_hdr->buf_hdr, (odp_buffer_seg_t)seg,
				  &seglen, pkt_hdr->frame_len,
				  pkt_hdr->headroom);

		if (ptr) {
			iovecs[iov_count].iov_base = ptr;
			iovecs[iov_count].iov_len = seglen;
			iov_count++;
		}
		seg = odp_packet_next_seg(pkt, seg);
	}

	return iov_count;
}

#ifdef __cplusplus
}
#endif

#endif
