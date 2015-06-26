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

#include <odp/config.h>
#include <odp/hints.h>

#define MAX_PKTIO_NAMESIZE 256

#define ETH_ALEN 6


/**
 * Packet IO types
 */
typedef enum {
	ODP_PKTIO_TYPE_START = 0x1,
	ODP_PKTIO_TYPE_LOOPBACK = 0x1,
	ODP_PKTIO_TYPE_MAGIC,
	ODP_PKTIO_TYPE_CLUSTER,
	ODP_PKTIO_TYPE_COUNT,
	//~ ODP_PKTIO_TYPE_IOCLUS,
	//~ ODP_PKTIO_TYPE_ETH,
	//~ ODP_PKTIO_TYPE_ETH40G,
} odp_pktio_type_t;

typedef struct {
	int clus_id;			/**< Cluster ID */
	odp_pool_t pool; 		/**< pool to alloc packets from */
	size_t max_frame_len; 		/**< max frame len = buf_size - sizeof(pkt_hdr) */
	size_t buf_size; 		/**< size of buffer payload in 'pool' */
	uint64_t sent_pkt_count;	/**< Count of packet sent to the clusters */
	uint64_t recv_pkt_count;	/**< Count of packet received */
} pktio_cluster_t;

typedef struct {
	char name[MAX_PKTIO_NAMESIZE];	/**< True name of pktio */
	int fd;				/**< magic syscall eth interface file descriptor */
	odp_pool_t pool; 		/**< pool to alloc packets from */
	size_t max_frame_len; 		/**< max frame len = buf_size - sizeof(pkt_hdr) */
	size_t buf_size; 		/**< size of buffer payload in 'pool' */
} pktio_magic_t;

typedef struct {
	char name[MAX_PKTIO_NAMESIZE];	/**< True name of pktio */
	int fd;
	odp_pool_t pool; 		/**< pool to alloc packets from */
	size_t max_frame_len; 		/**< max frame len = buf_size - sizeof(pkt_hdr) */
	size_t buf_size; 		/**< size of buffer payload in 'pool' */
	odp_queue_t loopq;		/**< loopback queue for "loop" device */
} pktio_loopback_t;

struct pktio_entry {
	odp_rwlock_t lock;		/**< entry RW lock */
	int taken;			/**< is entry taken(1) or free(0) */
	int cls_enabled;		/**< is classifier enabled */
	odp_pktio_t handle;		/**< pktio handle */
	odp_queue_t inq_default;	/**< default input queue, if set */
	odp_queue_t outq_default;	/**< default out queue */
	odp_pktio_type_t type;		/**< pktio type */
	classifier_t cls;		/**< classifier linked with this pktio*/
	char name[MAX_PKTIO_NAMESIZE];		/**< name of pktio provided to
										   pktio_open() */
	odp_bool_t promisc;		/**< promiscuous mode state */

	union {
		pktio_magic_t magic;
		pktio_loopback_t loop;
		pktio_cluster_t cluster;
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

struct pktio_if_operation {
	const char *name;
	int (* global_init)(void);
	int (* setup_pktio_entry)(pktio_entry_t * /* pktio_entry */, odp_pool_t /* pool */);
	void (* mac_get)(const pktio_entry_t * const /* pktio_entry */, void * /* mac_addr */);
	int (* recv)(pktio_entry_t * const /* pktio_entry */, odp_packet_t [] /* pkt_table */, int /* len */);
	int (* send)(pktio_entry_t * const /* pktio_entry */, odp_packet_t [] /* pkt_table */, unsigned /* len */);
	int (* promisc_mode_set)(pktio_entry_t * const /* pktio_entry */,  int /* enable */);
	int (* promisc_mode_get)(pktio_entry_t * const/* pktio_entry */);
	/**
	 * Open return -1 if the devname was not handled, 0 if handled with success and 1 if handled with error
	 */
	int (* open)(pktio_entry_t * const /* pktio_entry */, const char * /* devname */);
	int (* mtu_get)(pktio_entry_t * const /* pktio_entry */);
};

struct pktio_if_operation magic_pktio_operation;
struct pktio_if_operation loop_pktio_operation;
struct pktio_if_operation cluster_pktio_operation;

__attribute__ ((unused))
static const struct pktio_if_operation *pktio_if_ops[ODP_PKTIO_TYPE_COUNT] = {
	[ODP_PKTIO_TYPE_LOOPBACK] = &loop_pktio_operation,
	[ODP_PKTIO_TYPE_MAGIC] = &magic_pktio_operation,
	[ODP_PKTIO_TYPE_CLUSTER] = &cluster_pktio_operation,
};

#ifdef __cplusplus
}
#endif

#endif
