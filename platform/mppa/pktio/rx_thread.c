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
#include <mppa_noc.h>

#ifdef K1_NODEOS
#include <pthread.h>
#else
#include <utask.h>
#endif

#include "odp_pool_internal.h"
#include "odp_rx_internal.h"

#define MAX_RX_P_LINK 12
#define PKT_BURST_SZ (MAX_RX_P_LINK / N_RX_THR)

/** Per If data */
typedef struct rx_thread_if_data {
	odp_packet_t pkts[MAX_RX_P_LINK]; /**< PKT mapped to Rx tags */
	odp_bool_t broken[MAX_RX_P_LINK]; /**< Is Rx currently broken */
	uint64_t dropped_pkts[N_RX_THR];

	unsigned ev_masks[N_EV_MASKS];    /**< Mask to isolate events that
					   * belong to us */
	uint8_t pool_id;
	rx_config_t rx_config;
} rx_thread_if_data_t;

typedef struct {
	odp_packet_t spares[PKT_BURST_SZ * MAX_RX_IF];
	int n_spares;
	int n_rx;
} rx_pool_t;

/** Per thread data */
typedef struct rx_thread_data {
	uint8_t min_mask;               /**< Rank of minimum non-null mask */
	uint8_t max_mask;               /**< Rank of maximum non-null mask */
	uint64_t ev_masks[4];           /**< Mask to isolate events that belong
					 *   to us */
	rx_pool_t pools[ODP_CONFIG_POOLS];
} rx_thread_data_t;

typedef struct rx_thread {
	odp_rwlock_t lock;		/**< entry RW lock */
	int dma_if;                     /**< DMA interface being watched */

	odp_packet_t drop_pkt;          /**< ODP Packet used to temporary store
					 *   dropped data */
	uint8_t *drop_pkt_ptr;          /**< Pointer to drop_pkt buffer */
	uint32_t drop_pkt_len;          /**< Size of drop_pkt buffer in bytes */

	uint8_t tag2id[256];             /**< LUT to convert Rx Tag
					    to If Id */

	rx_thread_if_data_t if_data[MAX_RX_IF];
	rx_thread_data_t th_data[N_RX_THR];
} rx_thread_t;

static rx_thread_t rx_thread_hdl;

static inline int MIN(int a, int b)
{
	return a > b ? b : a;
}

static inline int MAX(int a, int b)
{
	return b > a ? b : a;
}

static int _configure_rx(rx_config_t *rx_config, int rx_id)
{
	odp_packet_t pkt = _odp_packet_alloc(rx_config->pool);
	odp_packet_hdr_t *pkt_hdr = odp_packet_hdr(pkt);

	if (pkt == ODP_PACKET_INVALID)
		return -1;

	rx_thread_hdl.if_data[rx_config->pktio_id].
		pkts[rx_id - rx_config->min_port] = pkt;

	int ret;
	uint32_t len;
	uint8_t * base_addr = packet_map(pkt_hdr, 0, &len);
	mppa_noc_dnoc_rx_configuration_t conf = {
		.buffer_base = (unsigned long)base_addr - rx_config->header_sz,
		.buffer_size = len + rx_config->header_sz,
		.current_offset = 0,
		.event_counter = 0,
		.item_counter = 1,
		.item_reload = 1,
		.reload_mode = MPPA_DNOC_DECR_NOTIF_RELOAD_ETH,
		.activation = 0x3,
		.counter_id = 0
	};

	ret = mppa_noc_dnoc_rx_configure(rx_config->dma_if, rx_id, conf);
	ODP_ASSERT(!ret);

	ret = mppa_dnoc[rx_config->dma_if]->rx_queues[rx_id].
		get_drop_pkt_nb_and_activate.reg;
	mppa_noc_enable_event(rx_config->dma_if,
			      MPPA_NOC_INTERRUPT_LINE_DNOC_RX,
			      rx_id, (1 << BSP_NB_PE_P) - 1);

	return 0;
}

static int _reload_rx(rx_thread_t *th, int th_id, int rx_id,
		      odp_packet_t spare, odp_packet_t *rx_pkt)
{
	int pktio_id = th->tag2id[rx_id];
	rx_thread_if_data_t *if_data = &th->if_data[pktio_id];
	int rank = rx_id - if_data->rx_config.min_port;
	odp_packet_t pkt = if_data->pkts[rank];
	odp_packet_t new_pkt = spare;
	int ret = 1;
	const rx_config_t *rx_config = &if_data->rx_config;

	*rx_pkt = pkt;

	if (pkt == ODP_PACKET_INVALID)
		if_data->dropped_pkts[th_id]++;

	typeof(mppa_dnoc[0]->rx_queues[0]) * const rx_queue =
		&mppa_dnoc[th->dma_if]->rx_queues[rx_id];

	if (new_pkt == ODP_PACKET_INVALID) {
		/* No packets were available. Map small dirty
		 * buffer to receive NoC packet but drop
		 * the frame */
		rx_queue->buffer_base.dword = (unsigned long)th->drop_pkt_ptr;
		rx_queue->buffer_size.dword = th->drop_pkt_len;
		/* We willingly do not change the offset here as we want
		 * to spread DMA Rx within the drop_pkt buffer */
	} else {
		/* Map the buffer in the DMA Rx */
		odp_packet_hdr_t *pkt_hdr = odp_packet_hdr(new_pkt);

		rx_queue->buffer_base.dword = (unsigned long)
			((uint8_t *)(pkt_hdr->buf_hdr.addr) +
			 pkt_hdr->headroom - rx_config->header_sz);

		/* Rearm the DMA Rx and check for droppped packets */
		rx_queue->current_offset.reg = 0ULL;

		rx_queue->buffer_size.dword = pkt_hdr->frame_len +
			1 * rx_config->header_sz;
	}

	int dropped = rx_queue->
		get_drop_pkt_nb_and_activate.reg;

	if (dropped) {
		/* We dropped some we need to try and
		 * drop more to get better */

		/* Put back a dummy buffer.
		 * We will drop those next ones anyway ! */
		rx_queue->buffer_base.dword = (unsigned long)th->drop_pkt_ptr;
		rx_queue->buffer_size.dword = th->drop_pkt_len;

		/* Really force those values.
		 * Item counter must be 2 in this case. */
		int j;

		for (j = 0; j < 16; ++j)
			rx_queue->item_counter.reg = 2;
		for (j = 0; j < 16; ++j)
			rx_queue->activation.reg = 0x1;

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
			uint8_t * const base_addr =
				((uint8_t *)pkt_hdr->buf_hdr.addr) +
				pkt_hdr->headroom;

			_odp_packet_reset_parse(pkt);

			switch (rx_config->if_type) {
			case RX_IF_TYPE_ETH: {
				uint8_t * const hdr_addr = base_addr -
					sizeof(mppa_ethernet_header_t);
				mppa_ethernet_header_t * const header =
					(mppa_ethernet_header_t *)hdr_addr;

				INVALIDATE(header);

				const unsigned len = header->info._.pkt_size -
					1 * sizeof(mppa_ethernet_header_t);
				packet_set_len(pkt, len);
			}
				break;
			case RX_IF_TYPE_PCI:
				assert(0);
			}
		}
	}
	return ret;
}

static void _poll_mask(rx_thread_t *th, int th_id,
		      odp_buffer_hdr_t *hdr_tbl[][QUEUE_MULTI_MAX],
		      int nbr_qentry[MAX_RX_IF], int rx_id)
{
	const int pktio_id = th->tag2id[rx_id];
	const rx_thread_if_data_t *if_data = &th->if_data[pktio_id];
	uint16_t ev_counter =
		mppa_noc_dnoc_rx_lac_event_counter(th->dma_if, rx_id);

	rx_pool_t * rx_pool = &th->th_data[th_id].pools[if_data->pool_id];

	/* Weird... No data ! */
	if (!ev_counter)
		return;

	if (!rx_pool->n_spares) {
		/* Alloc */
		struct pool_entry_s *entry =
			&((pool_entry_t *)if_data->rx_config.pool)->s;

		rx_pool->n_spares =
			get_buf_multi(entry,
				      (odp_buffer_hdr_t **)rx_pool->spares,
				      rx_pool->n_rx);
	}
	odp_packet_t pkt;

	if (rx_pool->n_spares) {
		rx_pool->n_spares -=
			_reload_rx(th, th_id, rx_id,
				   rx_pool->spares[rx_pool->n_spares - 1],
				   &pkt);
	} else {
		_reload_rx(th, th_id, rx_id,
			   ODP_PACKET_INVALID, &pkt);
	}

	if (pkt == ODP_PACKET_INVALID)
		/* Packet was corrupted */
		return;

	hdr_tbl[pktio_id][nbr_qentry[pktio_id]++] = (odp_buffer_hdr_t *)pkt;

	if (MAX_RX_P_LINK / N_RX_THR > QUEUE_MULTI_MAX &&
	    nbr_qentry[pktio_id] == QUEUE_MULTI_MAX){
		queue_entry_t *qentry;

		qentry = queue_to_qentry(if_data->rx_config.queue);
		queue_enq_multi(qentry, hdr_tbl[pktio_id], QUEUE_MULTI_MAX);
		nbr_qentry[pktio_id] = 0;
	}

	return;
}

static void _poll_masks(rx_thread_t *th, int th_id)
{
	int i;
	uint64_t mask = 0;

	odp_buffer_hdr_t *hdr_tbl[MAX_RX_IF][QUEUE_MULTI_MAX];
	int nbr[MAX_RX_IF] = {0};
	const rx_thread_data_t *th_data = &th->th_data[th_id];

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
			_poll_mask(th, th_id, hdr_tbl, nbr, rx_id);
		}
	}
	for (i = 0; i < MAX_RX_IF; ++i) {
		queue_entry_t *qentry;

		if (!nbr[i])
			continue;
		qentry = queue_to_qentry(th->if_data[i].rx_config.queue);
		queue_enq_multi(qentry, hdr_tbl[i], nbr[i]);
	}
	return;
}

static void *_rx_thread_start(void *arg)
{
	rx_thread_t *th = &rx_thread_hdl;
	int th_id = (unsigned long)(arg);

	while (1) {
		odp_rwlock_read_lock(&th->lock);
		INVALIDATE(th);
		_poll_masks(th, th_id);
		odp_rwlock_read_unlock(&th->lock);
	}
	return NULL;
}

int rx_thread_link_open(rx_config_t *rx_config, int n_ports)
{
	rx_thread_if_data_t *if_data =
		&rx_thread_hdl.if_data[rx_config->pktio_id];
	char loopq_name[ODP_QUEUE_NAME_LEN];

	snprintf(loopq_name, sizeof(loopq_name), "%d-pktio_rx",
		 rx_config->pktio_id);
	rx_config->queue = odp_queue_create(loopq_name, ODP_QUEUE_TYPE_POLL,
					    NULL);
	if (rx_config->queue == ODP_QUEUE_INVALID)
		return -1;

	/*
	 * Allocate contiguous RX ports
	 */
	int first_rx = -1;
	int n_rx;

	for (n_rx = 0; n_rx < n_ports; ++n_rx) {
		mppa_noc_ret_t ret;
		unsigned rx_port;

		ret = mppa_noc_dnoc_rx_alloc_auto(rx_config->dma_if,
						  &rx_port, MPPA_NOC_BLOCKING);
		if (ret != MPPA_NOC_RET_SUCCESS)
			break;

		if (first_rx >= 0 && (unsigned)(first_rx + n_rx) != rx_port) {
			/* Non contiguous port... Fail */
			mppa_noc_dnoc_rx_free(rx_config->dma_if, rx_port);
			break;
		}
		if (first_rx < 0)
			first_rx = rx_port;
	}

	if (n_rx < n_ports)
		goto err_free_rx;

	rx_config->min_port = first_rx;
	rx_config->max_port = first_rx + n_rx - 1;

	/*
	 * Compute event mask to detect events on our own tags later
	 */
	const uint64_t full_mask = 0xffffffffffffffffULL;
	const unsigned nrx_per_th = n_ports / N_RX_THR;
	uint64_t ev_masks[N_RX_THR][N_EV_MASKS];
	int i;

	for (i = 0; i < 4; ++i)
		if_data->ev_masks[i] = 0ULL;

	for (int th_id = 0; th_id < N_RX_THR; ++th_id) {
		int min_port = th_id * nrx_per_th + rx_config->min_port;
		int max_port = (th_id + 1) * nrx_per_th +
			rx_config->min_port - 1;

		for (i = 0; i < 4; ++i) {
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
				if_data->ev_masks[i] |= ev_masks[th_id][i];
		}
	}

	/* Copy config to Thread data */
	memcpy(&if_data->rx_config, rx_config, sizeof(*rx_config));
	if_data->pool_id = pool_to_id(rx_config->pool);

	for (i = rx_config->min_port; i <= rx_config->max_port; ++i)
		_configure_rx(rx_config, i);

	/* Push Context to handling threads */
	odp_rwlock_write_lock(&rx_thread_hdl.lock);
	INVALIDATE(&rx_thread_hdl);
	for (i = rx_config->min_port; i <= rx_config->max_port; ++i)
		rx_thread_hdl.tag2id[i] = rx_config->pktio_id;

	/* Allocate one packet to put all the broken ones
	 * coming from the NoC */
	if (rx_thread_hdl.drop_pkt == ODP_PACKET_INVALID) {
		odp_packet_hdr_t *hdr;

		rx_thread_hdl.drop_pkt = _odp_packet_alloc(rx_config->pool);
		hdr = odp_packet_hdr(rx_thread_hdl.drop_pkt);
		rx_thread_hdl.drop_pkt_ptr = hdr->buf_hdr.addr;
		rx_thread_hdl.drop_pkt_len = hdr->frame_len;
	}

	for (int i = 0; i < N_RX_THR; ++i) {
		rx_thread_data_t *th_data = &rx_thread_hdl.th_data[i];

		th_data->pools[if_data->pool_id].n_rx += nrx_per_th;

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
	odp_rwlock_write_unlock(&rx_thread_hdl.lock);
	return first_rx;

 err_free_rx:
	/* Last one was a failure or
	 * non contiguoues (thus freed already) */
	n_rx--;

	for ( ; n_rx >= 0; --n_rx)
		mppa_noc_dnoc_rx_free(rx_config->dma_if, first_rx + n_rx);
	return -1;
}

int rx_thread_link_close(uint8_t pktio_id)
{
	int i;
	rx_thread_if_data_t *if_data =
		&rx_thread_hdl.if_data[pktio_id];

	odp_rwlock_write_lock(&rx_thread_hdl.lock);
	INVALIDATE(if_data);
	odp_queue_destroy(if_data->rx_config.queue);

	for (i = if_data->rx_config.min_port;
	     i <= if_data->rx_config.max_port; ++i)
		rx_thread_hdl.tag2id[i] = -1;

	int n_ports = if_data->rx_config.max_port -
		if_data->rx_config.min_port + 1;
	const unsigned nrx_per_th = n_ports / N_RX_THR;

	if_data->rx_config.pool = ODP_POOL_INVALID;
	if_data->rx_config.min_port = -1;
	if_data->rx_config.max_port = -1;

	for (int i = 0; i < N_RX_THR; ++i) {
		rx_thread_data_t *th_data = &rx_thread_hdl.th_data[i];

		th_data->pools[if_data->pool_id].n_rx -= nrx_per_th;

		for (int j = 0; j < 4; ++j) {
			th_data->ev_masks[j] &= ~if_data->ev_masks[j];
			if (!th_data->ev_masks[j]) {
				if (j == th_data->min_mask)
					th_data->min_mask = j + 1;
				if (j == th_data->max_mask)
					th_data->max_mask = j - 1;
			}
		}
	}
	odp_rwlock_write_unlock(&rx_thread_hdl.lock);

	return 0;
}

int rx_thread_init(void)
{
	odp_rwlock_init(&rx_thread_hdl.lock);
	for (int i = 0; i < N_RX_THR; ++i) {
		/* Start threads */

#ifdef K1_NODEOS
		odp_cpumask_t thd_mask;
		pthread_attr_t attr;
		pthread_t thr;

		odp_cpumask_zero(&thd_mask);
		odp_cpumask_set(&thd_mask, BSP_NB_PE_P - i - 1);

		pthread_attr_init(&attr);
		pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t),
					    &thd_mask.set);

		if (pthread_create(&thr, &attr,
				   _rx_thread_start,
				   (void *)(unsigned long)(i)))
			ODP_ABORT("Thread failed");
#else
		utask_t task;

		if (utask_start_pe(&task, _rx_thread_start,
				   (void *)(unsigned long)(i),
				   BSP_NB_PE_P - i - 1))
			ODP_ABORT("Thread failed");
#endif
	}
	return 0;
}
