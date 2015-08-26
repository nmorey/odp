/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */
#include <odp_packet_io_internal.h>
#include <odp/thread.h>
#include <odp/cpumask.h>
#include "HAL/hal/hal.h"
#include <odp/errno.h>
#include <errno.h>

#ifdef K1_NODEOS
#include <pthread.h>
#else
#include <utask.h>
#endif

#include "odp_pool_internal.h"
#include "odp_rpc_internal.h"
#include "ucode_fw/ucode_eth.h"


#define MAX_ETH_SLOTS 2
#define MAX_ETH_PORTS 4
#define N_RX_P_ETH 12
#define MAX_ETH_IF ((MAX_ETH_SLOTS) * (MAX_ETH_PORTS))
#define PKT_BURST_SZ (N_RX_P_ETH / N_ETH_THR)

#define NOC_UC_COUNT		2
#define MAX_PKT_PER_UC		4
#define DNOC_CLUS_IFACE_ID	0

#if N_RX_P_ETH % N_ETH_THR != 0
#error "N_RX_P_ETH % N_ETH_THR != 0"
#endif

#include <mppa_noc.h>
#include <mppa_routing.h>

union mppa_ethernet_header_info_t {
 mppa_uint64 dword;
 mppa_uint32 word[2];
 mppa_uint16 hword[4];
 mppa_uint8 bword[8];
  struct {
    mppa_uint32 pkt_size : 16;
    mppa_uint32 hash_key : 16;
    mppa_uint32 lane_id  : 2;
    mppa_uint32 io_id    : 1;
    mppa_uint32 rule_id  : 4;
    mppa_uint32 pkt_id   : 25;
  } _;
};

struct mppa_ethernet_uc_ctx {
	unsigned int dnoc_tx_id;
	unsigned int dnoc_uc_id;
	bool is_running;
	odp_packet_t pkt_table[MAX_PKT_PER_UC];
	unsigned int pkt_count;
};

static struct mppa_ethernet_uc_ctx g_uc_ctx[NOC_UC_COUNT] = {{0}};

typedef struct mppa_ethernet_header_s {
  mppa_uint64 timestamp;
  union mppa_ethernet_header_info_t info;
} mppa_ethernet_header_t;

extern char _heap_end;

static int cluster_init_dnoc_tx(void)
{
	int i;
	mppa_noc_ret_t ret;
	mppa_noc_dnoc_uc_configuration_t uc_conf =
		MPPA_NOC_DNOC_UC_CONFIGURATION_INIT;

	uc_conf.program_start = (uintptr_t)ucode_eth;
	uc_conf.buffer_base = (uintptr_t)&_data_start;
	uc_conf.buffer_size = (uintptr_t)&_heap_end - (uintptr_t)&_data_start;

	for (i = 0; i < NOC_UC_COUNT; i++) {

		/* DNoC */
		ret = mppa_noc_dnoc_tx_alloc_auto(DNOC_CLUS_IFACE_ID,
						  &g_uc_ctx[i].dnoc_uc_id,
						  MPPA_NOC_BLOCKING);
		if (ret != MPPA_NOC_RET_SUCCESS)
			return 1;

		/* We will only use events */
		mppa_noc_disable_interrupt_handler(DNOC_CLUS_IFACE_ID,
						   MPPA_NOC_INTERRUPT_LINE_DNOC_TX,
						   g_uc_ctx[i].dnoc_uc_id);

		ret = mppa_noc_dnoc_uc_alloc_auto(DNOC_CLUS_IFACE_ID,
						  &g_uc_ctx[i].dnoc_uc_id,
						  MPPA_NOC_BLOCKING);
		if (ret != MPPA_NOC_RET_SUCCESS)
			return 1;

		ret = mppa_noc_dnoc_uc_link(DNOC_CLUS_IFACE_ID,
					    g_uc_ctx[i].dnoc_uc_id,
					    g_uc_ctx[i].dnoc_tx_id, uc_conf);
		if (ret != MPPA_NOC_RET_SUCCESS)
			return 1;
	}

	return 0;
}

typedef struct eth_status {
	odp_pool_t pool;             /**< pool to alloc packets from */
	odp_spinlock_t wlock;        /**< Tx lock */
	unsigned eth_if_id;
	odp_queue_t queue;           /**< Internal queue to store
				      *   received packets */

	/* Rx Data */
	odp_spinlock_t rlock;        /**< Rx lock */
	unsigned ev_masks[4];        /**< Mask to isolate events that belong
				      *   to us */

	uint8_t slot_id;             /**< IO Eth Id */
	uint8_t port_id;             /**< Eth Port id. 4 for 40G */
	uint8_t dma_if;              /**< DMA Rx Interface */
	uint8_t min_port;
	uint8_t max_port;

	/* Tx data */
	uint16_t tx_if;              /**< Remote DMA interface to forward
				      *   to Eth Egress */
	uint16_t tx_tag;             /**< Remote DMA tag to forward to
				      *   Eth Egress */
	mppa_dnoc_header_t header;
	mppa_dnoc_channel_config_t config;

	struct eth_thread *hdl;

} eth_status_t;

/** Per EthIf data */
typedef struct eth_thread_if_data {
	eth_status_t *eth;
	odp_pool_t pool;	       /**< pools to alloc packets from */
	odp_packet_t pkts[N_RX_P_ETH]; /**< Pointer to PKT mapped to Rx tags */
	odp_bool_t broken[N_RX_P_ETH]; /**< Pointer to PKT mapped to Rx tags */
	uint8_t min_port;
	uint8_t max_port;
	uint64_t dropped_pkts[N_ETH_THR];
} eth_thread_if_data_t;

/** Per thread data */
typedef struct eth_thread_data {
	uint8_t min_mask;               /**< Rank of minimum non-null mask */
	uint8_t max_mask;               /**< Rank of maximum non-null mask */
	uint64_t ev_masks[4];           /**< Mask to isolate events that belong
					 *   to us */
} eth_thread_data_t;

typedef struct eth_thread {
	odp_rwlock_t lock;		/**< entry RW lock */
	int dma_if;

	odp_packet_t drop_pkt;          /**< ODP Packet used to temporary store
					 *   dropped data */
	uint8_t *drop_pkt_ptr;          /**< Pointer to drop_pkt buffer */
	uint32_t drop_pkt_len;          /**< Size of drop_pkt buffer in bytes */

	uint8_t tag2id[256];             /**< LUT to convert Rx Tag
					    to Eth If Id */

	eth_thread_if_data_t if_data[MAX_ETH_IF];
	eth_thread_data_t th_data[N_ETH_THR];
} eth_thread_t;

static eth_thread_t eth_thread_hdl;

static int _eth_reload_rx(eth_thread_t *th, int th_id, int rx_id,
			  odp_packet_t spare, odp_packet_t *rx_pkt)
{
	int eth_id = th->tag2id[rx_id];
	eth_thread_if_data_t *if_data = &th->if_data[eth_id];
	int rank = rx_id - if_data->min_port;
	odp_packet_t pkt = if_data->pkts[rank];
	odp_packet_t new_pkt = spare;
	int ret = 1;

	*rx_pkt = pkt;

	if (pkt == ODP_PACKET_INVALID)
		if_data->dropped_pkts[th_id]++;

	if (new_pkt == ODP_PACKET_INVALID) {
		/* No packets were available. Map small dirty
		 * buffer to receive NoC packet but drop
		 * the Eth frame */
		mppa_dnoc[th->dma_if]->rx_queues[rx_id].buffer_base.dword =
			(unsigned long)th->drop_pkt_ptr;
		mppa_dnoc[th->dma_if]->rx_queues[rx_id].buffer_size.dword =
			th->drop_pkt_len;
		/* We willingly do not change the offset here as we want
		 * to spread DMA Rx within the drop_pkt buffer */
	} else {
		/* Map the buffer in the DMA Rx */
		odp_packet_hdr_t *pkt_hdr = odp_packet_hdr(new_pkt);

		mppa_dnoc[th->dma_if]->rx_queues[rx_id].buffer_base.dword =
			(unsigned long)((uint8_t *)(pkt_hdr->buf_hdr.addr) +
					pkt_hdr->headroom -
					sizeof(mppa_ethernet_header_t));
		/* Rearm the DMA Rx and check for droppped packets */
		mppa_dnoc[th->dma_if]->rx_queues[rx_id].current_offset.reg =
			0ULL;

		mppa_dnoc[th->dma_if]->rx_queues[rx_id].buffer_size.dword =
			pkt_hdr->frame_len + 2 * sizeof(mppa_ethernet_header_t);
	}

	int dropped = mppa_dnoc[th->dma_if]->rx_queues[rx_id].
		get_drop_pkt_nb_and_activate.reg;

	if (dropped) {
		/* We dropped some we need to try and
		 * drop more to get better */

		/* Put back a dummy buffer.
		 * We will drop those next ones anyway ! */
		mppa_dnoc[th->dma_if]->rx_queues[rx_id].buffer_base.dword =
			(unsigned long)th->drop_pkt_ptr;
		mppa_dnoc[th->dma_if]->rx_queues[rx_id].buffer_size.dword =
			th->drop_pkt_len;

		/* Really force those values.
		 * Item counter must be 2 in this case. */
		int j;

		for (j = 0; j < 16; ++j)
			mppa_dnoc[th->dma_if]->rx_queues[rx_id].
				item_counter.reg = 2;
		for (j = 0; j < 16; ++j)
			mppa_dnoc[th->dma_if]->rx_queues[rx_id].
				activation.reg = 0x1;

		/* +1 for the extra item counter we just configure.
		 * The second item counter
		 * will be counted by the pkt == ODP_PACKET_INVALID */
		if_data->dropped_pkts[th_id] += dropped + 1;
		if_data->broken[rank] = true;

		/* We didn't actually used the spare one */
		if_data->pkts[rank] = ODP_PACKET_INVALID;
		ret = 0;

		if (pkt != ODP_PACKET_INVALID) {
			/* If we pulled a packet, it has to be destroyed.
			 * Mark it as parsed with frame_len error */
			odp_packet_hdr_t *pkt_hdr = odp_packet_hdr(pkt);

			_odp_packet_reset_parse(pkt);
			pkt_hdr->error_flags.frame_len = 1;
			pkt_hdr->input_flags.unparsed = 0;
		}

	} else {
		if_data->broken[rank] = false;

		if_data->pkts[rank] = new_pkt;
		if (pkt != ODP_PACKET_INVALID) {
			/* Mark the new packet as unparsed and configure
			 * its length from the LB header */

			odp_packet_hdr_t *pkt_hdr = odp_packet_hdr(pkt);
			mppa_ethernet_header_t *header = (mppa_ethernet_header_t*)
				(((uint8_t*)pkt_hdr->buf_hdr.addr) +
				 pkt_hdr->headroom - sizeof(mppa_ethernet_header_t));
			INVALIDATE(header);
			_odp_packet_reset_parse(pkt);

			unsigned len = header->info._.pkt_size -
				2 * sizeof(mppa_ethernet_header_t);
			packet_set_len(pkt, len);
		}
	}
	return ret;
}

static int _eth_poll_mask(eth_thread_t *th, int th_id,
			  odp_packet_t spares[PKT_BURST_SZ], int n_spares,
			  odp_buffer_hdr_t *hdr_tbl[][QUEUE_MULTI_MAX],
			  int nbr_qentry[MAX_ETH_IF], int rx_id)
{
	const int eth_if = th->tag2id[rx_id];
	const eth_thread_if_data_t *if_data = &th->if_data[eth_if];
	uint16_t ev_counter =
		mppa_noc_dnoc_rx_lac_event_counter(th->dma_if, rx_id);

	/* Weird... No data ! */
	if (!ev_counter)
		return n_spares;

	if (!n_spares) {
		/* Alloc */
		n_spares = get_buf_multi(&((pool_entry_t *)if_data->pool)->s,
					 (odp_buffer_hdr_t **)spares,
					 PKT_BURST_SZ);
	}
	odp_packet_t pkt;

	if (n_spares) {
		n_spares -= _eth_reload_rx(th, th_id, rx_id,
					   spares[n_spares - 1], &pkt);
	} else {
		_eth_reload_rx(th, th_id, rx_id,
			       ODP_PACKET_INVALID, &pkt);
	}

	if (pkt == ODP_PACKET_INVALID)
		/* Packet was corrupted */
		return n_spares;

	hdr_tbl[eth_if][nbr_qentry[eth_if]++] = (odp_buffer_hdr_t *)pkt;

	if (N_RX_P_ETH / N_ETH_THR > QUEUE_MULTI_MAX &&
	    nbr_qentry[eth_if] == QUEUE_MULTI_MAX){
		queue_entry_t *qentry;

		qentry = queue_to_qentry(if_data->eth->queue);
		queue_enq_multi(qentry, hdr_tbl[eth_if], QUEUE_MULTI_MAX);
		nbr_qentry[eth_if] = 0;
	}

	return n_spares;
}

static int _eth_poll_masks(eth_thread_t *th, int th_id,
			   odp_packet_t spares[PKT_BURST_SZ], int n_spares)
{
	int i;
	uint64_t mask = 0;

	odp_buffer_hdr_t *hdr_tbl[MAX_ETH_IF][QUEUE_MULTI_MAX];
	int nbr[MAX_ETH_IF] = {0};
	const eth_thread_data_t *th_data = &th->th_data[th_id];

	for (i = th_data->min_mask; i <= th_data->max_mask; ++i) {
		mask = mppa_dnoc[th->dma_if]->rx_global.events[i].dword &
			th_data->ev_masks[i];

		if (mask == 0ULL)
			continue;

		/* We have an event */
		while (mask != 0ULL) {
			const int mask_bit = __k1_ctz(mask);
			const int rx_id = mask_bit + i * 64;

			mask = mask ^ (1ULL << mask_bit);
			n_spares = _eth_poll_mask(th, th_id, spares, n_spares,
						  hdr_tbl, nbr, rx_id);
		}
	}
	for (i = 0; i < MAX_ETH_IF; ++i) {
		queue_entry_t *qentry;

		if (!nbr[i])
			continue;
		qentry = queue_to_qentry(th->if_data[i].eth->queue);
		queue_enq_multi(qentry, hdr_tbl[i], nbr[i]);
	}
	return n_spares;
}

static void *_eth_thread(void *arg)
{
	eth_thread_t *th = &eth_thread_hdl;
	int th_id = (unsigned long)(arg);
	odp_packet_t spares[PKT_BURST_SZ];
	int n_spares = 0;

	while (1) {
		odp_rwlock_read_lock(&th->lock);
		INVALIDATE(th);
		n_spares = _eth_poll_masks(th, th_id, spares, n_spares);
		odp_rwlock_read_unlock(&th->lock);
	}
	return NULL;
}

/**
 * #############################
 * PKTIO Interface
 * #############################
 */
static inline int MIN(int a, int b)
{
	return a > b ? b : a;
}

static inline int MAX(int a, int b)
{
	return b > a ? b : a;
}

static int eth_init(void)
{
	odp_rwlock_init(&eth_thread_hdl.lock);

	cluster_init_dnoc_tx();
	for (int i = 0; i < N_ETH_THR; ++i) {
		/* Start threads */

#ifdef K1_NODEOS
		odp_cpumask_t thd_mask;
		pthread_attr_t attr ;
		pthread_t thr;

		odp_cpumask_zero(&thd_mask);
		odp_cpumask_set(&thd_mask, BSP_NB_PE_P - i - 1);

		pthread_attr_init(&attr);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t),
					    &thd_mask.set);

		if(pthread_create(&thr, &attr,
				  _eth_thread, (void*)(unsigned long)(i)))
			ODP_ABORT("Thread failed");
#else
		utask_t task;
		if (utask_start_pe(&task, _eth_thread,
				   (void *)(unsigned long)(i),
				   BSP_NB_PE_P - i - 1))
			ODP_ABORT("Thread failed");
#endif
	}
	return 0;
}

static void _eth_set_rx_conf(unsigned if_id, int rx_id, odp_packet_t pkt)
{
	odp_packet_hdr_t * pkt_hdr = odp_packet_hdr(pkt);
	mppa_noc_dnoc_rx_configuration_t conf = {
		.buffer_base = (unsigned long)packet_map(pkt_hdr, 0, NULL) -
		sizeof(mppa_ethernet_header_t),
		.buffer_size = pkt_hdr->frame_len +
		2 * sizeof(mppa_ethernet_header_t),
		.current_offset = 0,
		.event_counter = 0,
		.item_counter = 1,
		.item_reload = 1,
		.reload_mode = MPPA_DNOC_DECR_NOTIF_RELOAD_ETH,
		.activation = 0x3,
		.counter_id = 0
	};

	int ret = mppa_noc_dnoc_rx_configure(if_id, rx_id, conf);
	ODP_ASSERT(!ret);
	ret = mppa_dnoc[if_id]->rx_queues[rx_id].
		get_drop_pkt_nb_and_activate.reg;
}

static int _eth_configure_rx(eth_status_t *eth, int rx_id)
{
	odp_packet_t pkt = _odp_packet_alloc(eth->pool);
	if (pkt == ODP_PACKET_INVALID)
		return -1;

	eth->hdl->if_data[eth->eth_if_id].pkts[rx_id - eth->min_port] = pkt;
	_eth_set_rx_conf(eth->dma_if, rx_id, pkt);

	mppa_noc_enable_event(eth->dma_if, MPPA_NOC_INTERRUPT_LINE_DNOC_RX,
			      rx_id, (1 << BSP_NB_PE_P) - 1);

	return 0;
}

static int eth_open(odp_pktio_t id ODP_UNUSED, pktio_entry_t *pktio_entry,
		    const char *devname, odp_pool_t pool)
{

	/*
	 * Check device name and extract slot/port
	 */
	if (devname[0] != 'p')
		return -1;

	int slot_id = devname[1] - '0';
	int port_id = 4;
	if (slot_id < 0 || slot_id >= MAX_ETH_SLOTS)
		return -1;

	if (devname[2] != 0) {
		if(devname[2] != 'p')
			return -1;
		port_id = devname[3] - '0';

		if (port_id < 0 || port_id >= MAX_ETH_PORTS)
			return -1;

		if(devname[4] != 0)
			return -1;
	}

	pktio_entry->s.pkt_eth.status = malloc(sizeof(*pktio_entry->s.pkt_eth.status));
	if (!pktio_entry->s.pkt_eth.status)
		return -1;

	eth_thread_t *hdl = &eth_thread_hdl;
	eth_status_t *eth = pktio_entry->s.pkt_eth.status;
	/*
	 * Init eth status
	 */
	eth->pool = pool;
	eth->slot_id = slot_id;
	eth->port_id = port_id;
	eth->dma_if = 0;
	eth->eth_if_id = slot_id * MAX_ETH_PORTS + port_id;
	eth->hdl = hdl;

	odp_spinlock_init(&eth->rlock);
	odp_spinlock_init(&eth->wlock);

	char loopq_name[ODP_QUEUE_NAME_LEN];

	snprintf(loopq_name, sizeof(loopq_name), "%" PRIu64 "-pktio_ethq",
		 odp_pktio_to_u64(id));
	eth->queue = odp_queue_create(loopq_name, ODP_QUEUE_TYPE_POLL, NULL);
	if (eth->queue == ODP_QUEUE_INVALID)
		return -1;

	/*
	 * Allocate contiguous RX ports
	 */
	int first_rx = -1;
	int n_rx;
	for (n_rx = 0; n_rx < N_RX_P_ETH; ++n_rx) {
		mppa_noc_ret_t ret;
		unsigned rx_port;

		ret = mppa_noc_dnoc_rx_alloc_auto(eth->dma_if, &rx_port, MPPA_NOC_BLOCKING);
		if(ret != MPPA_NOC_RET_SUCCESS)
			break;

		if (first_rx >= 0 && (unsigned)(first_rx + n_rx) != rx_port) {
			/* Non contiguous port... Fail */
			mppa_noc_dnoc_rx_free(eth->dma_if, rx_port);
			break;
		}
		if(first_rx < 0)
			first_rx = rx_port;
	}
	if (n_rx < N_RX_P_ETH) {
		/* Something went wrong. Free the ports */

		/* Last one was a failure or
		 * non contiguoues (thus freed already) */
		n_rx --;

		for ( ;n_rx >= 0; --n_rx)
			mppa_noc_dnoc_rx_free(eth->dma_if, first_rx + n_rx);
	}

	eth->min_port = first_rx;
	eth->max_port = first_rx + n_rx - 1;

	/*
	 * Compute event mask to detect events on our own tags later
	 */
	const uint64_t full_mask = 0xffffffffffffffffULL;
	const unsigned nrx_per_th = N_RX_P_ETH / N_ETH_THR;
	uint64_t ev_masks[N_ETH_THR][4];

	for (int i = 0; i < 4; ++i)
		eth->ev_masks[i] = 0ULL;

	for (int th_id = 0; th_id < N_ETH_THR; ++th_id) {
		int min_port = th_id * nrx_per_th + eth->min_port;
		int max_port = (th_id + 1) * nrx_per_th + eth->min_port - 1;

		for (int i = 0; i < 4; ++i) {
			if (min_port >= (i + 1) * 64 || max_port < i * 64) {
				ev_masks[th_id][i] = 0ULL;
				continue;
			}
			uint8_t local_min = MAX(i * 64, min_port) - (i * 64);
			uint8_t local_max =
				MIN((i + 1) * 64 - 1, max_port) - (i * 64);

			ev_masks[th_id][i] =
				(/* Trim the upper bits */
				 (
				  /* Trim the lower bits */
				  full_mask >> (local_min)
				  )
				 /* Realign back + trim the top */
				 << (local_min + 63 - local_max)
				 ) /* Realign again */ >> (63 - local_max);

			if (ev_masks[th_id][i] != 0)
				eth->ev_masks[i] |= ev_masks[th_id][i];
		}
	}

	for (int i = eth->min_port; i <= eth->max_port; ++i) {
		_eth_configure_rx(eth, i);
	}

	/* Push Context to handling threads */
	odp_rwlock_write_lock(&hdl->lock);
	INVALIDATE(hdl);
	for (int i = eth->min_port; i <= eth->max_port; ++i)
		hdl->tag2id[i] = eth->eth_if_id;

	eth_thread_if_data_t *if_data = &hdl->if_data[eth->eth_if_id];

	if_data->eth = eth;
	if_data->pool = pool;
	if_data->min_port = eth->min_port;
	if_data->max_port = eth->max_port;

	/* Allocate one packet to put all the broken ones
	 * coming from the NoC */
	if (hdl->drop_pkt == ODP_PACKET_INVALID) {
		odp_packet_hdr_t *hdr;

		hdl->drop_pkt = _odp_packet_alloc(pool);
		hdr = odp_packet_hdr(hdl->drop_pkt);
		hdl->drop_pkt_ptr = hdr->buf_hdr.addr;
		hdl->drop_pkt_len = hdr->frame_len;
	}

	for (int i = 0; i < N_ETH_THR; ++i) {
		eth_thread_data_t *th_data = &hdl->th_data[i];

		for (int j = 0; j < 4; ++j) {
			th_data->ev_masks[j] |= ev_masks[i][j];
			if (ev_masks[i][j]) {
				if (j < th_data->min_mask)
					th_data->min_mask = j;
				if (j > th_data->max_mask)
					th_data->max_mask = j;
			}
		}
	}
	odp_rwlock_write_unlock(&hdl->lock);
	/*
	 * RPC Msg to IOETH  #N so the LB will dispatch to us
	 */
	odp_rpc_cmd_open_t open_cmd = {
		{
			.ifId = port_id,
			.dma_if = eth->dma_if,
			.min_rx = eth->min_port,
			.max_rx = eth->max_port,
		}
	};
	unsigned cluster_id = __k1_get_cluster_id();
	odp_rpc_t cmd = {
		.pkt_type = ODP_RPC_CMD_ETH_OPEN,
		.data_len = 0,
		.flags = 0,
		.inl_data = open_cmd.inl_data
	};
	odp_rpc_t *ack_msg;
	odp_rpc_cmd_ack_t ack;

	odp_rpc_do_query(odp_rpc_get_ioeth_dma_id(slot_id, cluster_id),
			 odp_rpc_get_ioeth_tag_id(slot_id, cluster_id),
			 &cmd, NULL);
	odp_rpc_wait_ack(&ack_msg, NULL);
	ack.inl_data = ack_msg->inl_data;
	if (ack.status) {
		fprintf(stderr, "[ETH] Error: Server declined opening of '%s'\n", devname);
	}

	eth->tx_if = ack.open.eth_tx_if;
	eth->tx_tag = ack.open.eth_tx_tag;

	mppa_routing_get_dnoc_unicast_route(__k1_get_cluster_id(), eth->tx_if,
					    &eth->config, &eth->header);

	eth->config._.loopback_multicast = 0;
	eth->config._.cfg_pe_en = 1;
	eth->config._.cfg_user_en = 1;
	eth->config._.write_pe_en = 1;
	eth->config._.write_user_en = 1;
	eth->config._.decounter_id = 0;
	eth->config._.decounted = 0;
	eth->config._.payload_min = 1;
	eth->config._.payload_max = 32;
	eth->config._.bw_current_credit = 0xff;
	eth->config._.bw_max_credit     = 0xff;
	eth->config._.bw_fast_delay     = 0x00;
	eth->config._.bw_slow_delay     = 0x00;

	eth->header._.tag = eth->tx_tag;
	eth->header._.valid = 1;

	return ack.status;
}

static int eth_close(pktio_entry_t * const pktio_entry)
{

	eth_status_t * eth = pktio_entry->s.pkt_eth.status;
	int slot_id = eth->slot_id;
	int port_id = eth->port_id;
	odp_rpc_t *ack_msg;
	odp_rpc_cmd_ack_t ack;

	odp_rpc_cmd_clos_t close_cmd = {
		{
			.ifId = eth->port_id = port_id

		}
	};
	unsigned cluster_id = __k1_get_cluster_id();
	odp_rpc_t cmd = {
		.pkt_type = ODP_RPC_CMD_ETH_CLOS,
		.data_len = 0,
		.flags = 0,
		.inl_data = close_cmd.inl_data
	};

	odp_rpc_do_query(odp_rpc_get_ioeth_dma_id(slot_id, cluster_id),
			 odp_rpc_get_ioeth_tag_id(slot_id, cluster_id),
			 &cmd, NULL);

	odp_rpc_wait_ack(&ack_msg, NULL);
	ack.inl_data = ack_msg->inl_data;

	odp_queue_destroy(eth->queue);
	/* Push Context to handling threads */

	eth_thread_t *hdl = eth->hdl;
	eth_thread_if_data_t *if_data = &hdl->if_data[eth->eth_if_id];

	odp_rwlock_write_lock(&hdl->lock);
	INVALIDATE(hdl);
	for (int i = eth->min_port; i <= eth->max_port; ++i)
		hdl->tag2id[i] = -1;

	if_data->eth = NULL;
	if_data->pool = ODP_POOL_INVALID;
	if_data->min_port = -1;
	if_data->max_port = -1;

	for (int i = 0; i < N_ETH_THR; ++i) {
		eth_thread_data_t *th_data = &hdl->th_data[i];

		for (int j = 0; j < 4; ++j) {
			th_data->ev_masks[j] &= ~eth->ev_masks[j];
			if (!th_data->ev_masks[j]) {
				if (j == th_data->min_mask)
					th_data->min_mask = j + 1;
				if (j == th_data->max_mask)
					th_data->max_mask = j - 1;
			}
		}
	}
	odp_rwlock_write_unlock(&hdl->lock);

	free(eth);
	pktio_entry->s.pkt_eth.status = NULL;
	return ack.status;
}

static int eth_mac_addr_get(pktio_entry_t *pktio_entry ODP_UNUSED,
			    void *mac_addr ODP_UNUSED)
{
	/* FIXME */
	return -1;
}




static int eth_recv(pktio_entry_t *pktio_entry, odp_packet_t pkt_table[],
		    unsigned len)
{
	eth_status_t * eth = pktio_entry->s.pkt_eth.status;
	queue_entry_t *qentry;

	qentry = queue_to_qentry(eth->queue);
	return queue_deq_multi(qentry, (odp_buffer_hdr_t **)pkt_table, len);
}


static inline int
eth_send_packets(eth_status_t * eth, odp_packet_t pkt_table[], unsigned int pkt_count)
{
	mppa_noc_dnoc_uc_configuration_t uc_conf = MPPA_NOC_DNOC_UC_CONFIGURATION_INIT;
	mppa_noc_uc_program_run_t program_run = {{1, 1}};
	mppa_noc_ret_t nret;
	odp_packet_hdr_t * pkt_hdr;
	unsigned int i;
	mppa_noc_uc_pointer_configuration_t uc_pointers = {{0}};
	unsigned int tx_index = eth->port_id % NOC_UC_COUNT;

	/* Wait for previous run to complete */
	if (g_uc_ctx[tx_index].is_running) {
		mppa_noc_wait_clear_event(DNOC_CLUS_IFACE_ID,
					  MPPA_NOC_INTERRUPT_LINE_DNOC_TX,
					  g_uc_ctx[tx_index].dnoc_tx_id);
		/* Free previous packets */
		for(i = 0; i < pkt_count; i++)
			odp_packet_free(g_uc_ctx[tx_index].pkt_table[i]);
	}

	nret = mppa_noc_dnoc_uc_configure(DNOC_CLUS_IFACE_ID, g_uc_ctx[tx_index].dnoc_uc_id,
					  uc_conf, eth->header, eth->config);
	if (nret != MPPA_NOC_RET_SUCCESS)
		return 1;

	for(i = 0; i < pkt_count; i++) {
		pkt_hdr = odp_packet_hdr(pkt_table[i]);
		/* Setup parameters and pointers */
		uc_conf.parameters[i * 2] = pkt_hdr->frame_len / sizeof(uint64_t);
		uc_conf.parameters[i * 2 + 1] = pkt_hdr->frame_len % sizeof(uint64_t);
		uc_pointers.thread_pointers[i] = (uintptr_t) packet_map(pkt_hdr, 0, NULL) - (uintptr_t) &_data_start;

		/* Store current packet to free them later */
		g_uc_ctx[tx_index].pkt_table[i] = pkt_table[i];
	}

	uc_conf.pointers = &uc_pointers;
	uc_conf.event_counter = 0;

	g_uc_ctx[tx_index].pkt_count = pkt_count;
	g_uc_ctx[tx_index].is_running = 1;

	mppa_noc_dnoc_uc_set_program_run(DNOC_CLUS_IFACE_ID, g_uc_ctx[tx_index].dnoc_uc_id,
					 program_run);

	return 0;
}

static int eth_send(pktio_entry_t *pktio_entry, odp_packet_t pkt_table[],
		    unsigned len)
{
	unsigned int sent = 0;
	eth_status_t * eth = pktio_entry->s.pkt_eth.status;
	unsigned int pkt_count;

	odp_spinlock_lock(&eth->wlock);

	while(sent < len) {
		pkt_count = (len - sent) > MAX_PKT_PER_UC ? MAX_PKT_PER_UC : (len - sent);

		eth_send_packets(eth, &pkt_table[sent], pkt_count);
		sent += pkt_count;
	}

	odp_spinlock_unlock(&eth->wlock);

	return sent;
}

static int eth_promisc_mode_set(pktio_entry_t *const pktio_entry ODP_UNUSED,
				odp_bool_t enable ODP_UNUSED){
	return -1;
}

static int eth_promisc_mode(pktio_entry_t *const pktio_entry ODP_UNUSED){
	return -1;
}

static int eth_mtu_get(pktio_entry_t *const pktio_entry ODP_UNUSED) {
	return -1;
}
const pktio_if_ops_t eth_pktio_ops = {
	.init = eth_init,
	.term = NULL,
	.open = eth_open,
	.close = eth_close,
	.recv = eth_recv,
	.send = eth_send,
	.mtu_get = eth_mtu_get,
	.promisc_mode_set = eth_promisc_mode_set,
	.promisc_mode_get = eth_promisc_mode,
	.mac_get = eth_mac_addr_get,
};
