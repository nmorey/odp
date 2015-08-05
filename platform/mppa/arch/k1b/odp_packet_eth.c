/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */
#include <odp_packet_io_internal.h>
#include "HAL/hal/hal.h"
#include <odp/errno.h>
#include <errno.h>
#include "odp_rpc_internal.h"


#define MAX_ETH_SLOTS 2
#define MAX_ETH_PORTS 4
#define N_RX_PER_PORT 10

#include <mppa_noc.h>

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

typedef struct mppa_ethernet_header_s {
  mppa_uint64 timestamp;
  union mppa_ethernet_header_info_t info;
} mppa_ethernet_header_t;

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
	return 0;
}

typedef struct eth_status {
	odp_pool_t pool; 		 /**< pool to alloc packets from */
	odp_spinlock_t rlock;            /**< Rx lock */
	odp_spinlock_t wlock;            /**< Tx lock */

	unsigned ev_masks[8];            /**< Mask to isolate events that belong to us */
	odp_packet_t pkts[N_RX_PER_PORT];/**< Pointer to PKT mapped to Rx tags */
	uint64_t dropped_pkts;           /**< Number of droppes pkts */
	mppa_noc_dnoc_rx_bitmask_t bmask;/**< Last read bitmask to avoid starvation */

	uint8_t slot_id;                 /**< IO Eth Id */
	uint8_t port_id;                 /**< Eth Port id. 4 for 40G */
	uint8_t dma_if;                  /**< DMA Rx Interface */
	uint8_t min_port;                /**< Minimum port in the port range */
	uint8_t max_port;                /**< Maximum port in the port range */
	uint8_t min_mask;                /**< Rank of minimum non-null mask */
	uint8_t max_mask;                /**< Rank of maximum non-null mask */
	uint8_t refresh_rx;              /**< At least some Rx do not have any registered packets */
} eth_status_t;

static void _eth_set_rx_conf(unsigned ifId, int rxId, odp_packet_t pkt)
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
		.reload_mode = MPPA_NOC_RX_RELOAD_MODE_DECR_NOTIF_NO_RELOAD_IDLE,
		.activation = 0x3,
		.counter_id = 0
	};

	int ret = mppa_noc_dnoc_rx_configure(ifId, rxId, conf);
	ODP_ASSERT(!ret);
}

static int _eth_configure_rx(eth_status_t *eth, int rxId)
{
	odp_packet_t pkt = _odp_packet_alloc(eth->pool);
	if (pkt == ODP_PACKET_INVALID)
		return -1;

	eth->pkts[rxId - eth->min_port] = pkt;
	_eth_set_rx_conf(eth->dma_if, rxId, pkt);

	mppa_noc_enable_event(eth->dma_if, MPPA_NOC_INTERRUPT_LINE_DNOC_RX,
			      rxId, (1 << BSP_NB_PE_P) - 1);
	mppa_dnoc[eth->dma_if]->rx_queues[rxId].get_drop_pkt_nb_and_activate.reg;

	return 0;
}

static odp_packet_t _eth_reload_rx(eth_status_t *eth, int rxId)
{
	int rank = rxId - eth->min_port;
	odp_packet_t pkt = eth->pkts[rank];
	odp_packet_t new_pkt = ODP_PACKET_INVALID;

	if (pkt != ODP_PACKET_INVALID) {
		/* Compute real packet length */
		mppa_ethernet_header_t * header =
			(void*)(unsigned long)mppa_dnoc[eth->dma_if]->
			rx_queues[rxId].buffer_base.reg;

		packet_set_len(pkt, header->info._.pkt_size);
	}

	if(new_pkt == ODP_PACKET_INVALID)
		new_pkt = _odp_packet_alloc(eth->pool);
	eth->pkts[rank] = new_pkt;

	if (new_pkt == ODP_PACKET_INVALID) {
		eth->refresh_rx = 1;
		return pkt;
	}

	mppa_dnoc[eth->dma_if]->rx_queues[rxId].buffer_base.dword =
		(unsigned long)packet_map(odp_packet_hdr(new_pkt), 0, NULL) - sizeof(eth_status_t);
	mppa_dnoc[eth->dma_if]->rx_queues[rxId].current_offset.reg = 0ULL;

	eth->dropped_pkts += mppa_dnoc[eth->dma_if]->rx_queues[rxId].get_drop_pkt_nb_and_activate.reg;

	return pkt;
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

	eth_status_t * eth = pktio_entry->s.pkt_eth.status;
	/*
	 * Init eth status
	 */
	eth->pool = pool;
	eth->slot_id = slot_id;
	eth->port_id = port_id;
	eth->dma_if = 0;

	odp_spinlock_init(&eth->rlock);
	odp_spinlock_init(&eth->wlock);

	/*
	 * Allocate contiguous RX ports
	 */
	int first_rx = -1;
	int n_rx;
	for (n_rx = 0; n_rx < N_RX_PER_PORT; ++n_rx) {
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
	if (n_rx < N_RX_PER_PORT) {
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
	unsigned full_mask = (unsigned)(-1);

	for (int i = 0; i < 8; ++i){
		if (eth->min_port >= (i + 1) * 32 || eth->max_port < i * 32) {
			eth->ev_masks[i] = 0ULL;
			continue;
		}
		uint8_t local_min = MAX(i * 32, eth->min_port) - (i * 32);
		uint8_t local_max = MIN((i + 1) * 32 - 1, eth->max_port) - (i * 32);
		eth->ev_masks[i] = 
			(/* Trim the upper bits */
			 (
			  /* Trim the lower bits */
			  full_mask >> (local_min)
			  )
			 /* Realign back + trim the top */
			 << (local_min + 31 - local_max)
			 ) /* Realign again */ >> (31 - local_max);

		if (eth->ev_masks[i] != 0) {
			if (eth->min_mask == (uint8_t)-1)
				eth->min_mask = i;
			eth->max_mask = i;
		}
	}

	for (int i = eth->min_port; i <= eth->max_port; ++i) {
		_eth_configure_rx(eth, i);
	}

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
	odp_rpc_do_query(odp_rpc_get_ioeth_dma_id(slot_id, cluster_id),
			 odp_rpc_get_ioeth_tag_id(slot_id, cluster_id),
			 &cmd, NULL);

	odp_rpc_t * ack_msg;
	odp_rpc_wait_ack(&ack_msg, NULL);
	odp_rpc_cmd_ack_t ack = { .inl_data = ack_msg->inl_data};
	return ack.status;
}

static int eth_close(pktio_entry_t * const pktio_entry)
{

	eth_status_t * eth = pktio_entry->s.pkt_eth.status;
	int slot_id = eth->slot_id;
	int port_id = eth->port_id;

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

	odp_rpc_t * ack_msg;
	odp_rpc_wait_ack(&ack_msg, NULL);
	odp_rpc_cmd_ack_t ack = { .inl_data = ack_msg->inl_data};

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

static void _eth_reload_rxes(eth_status_t * eth)
{
	odp_packet_t pkt;

	eth->refresh_rx = 0;
	for (int i = 0; i < N_RX_PER_PORT && !eth->dropped_pkts; ++i) {
		if(eth->pkts[i] != ODP_PACKET_INVALID)
			continue;

		pkt = _eth_reload_rx(eth, eth->min_port + i);
		if(pkt != ODP_PACKET_INVALID)
			ODP_ERR("Invalid reloaded packet");
	}
}

static unsigned _eth_poll_mask(eth_status_t * eth, odp_packet_t pkt_table[],
			       unsigned len)
{
	unsigned nb_rx = 0;
	int i;
	uint32_t mask = 0;

	for (i = eth->min_mask; i <= eth->max_mask && nb_rx < len; ++i) {
		mask = eth->ev_masks[i] & eth->bmask.bitmask32[i];

		if (mask == 0ULL)
			continue;

		/* We have an event */
		while (mask != 0ULL && nb_rx < len) {
			int mask_bit = __k1_ctzdl(mask);
			int rx_id = mask_bit + i * 32;
			mask = mask ^ (1ULL << mask_bit);

			uint16_t ev_counter = mppa_noc_dnoc_rx_lac_event_counter(eth->dma_if, rx_id);
			/* Weird... No data ! */
			if(!ev_counter)
				continue;

			odp_packet_t pkt = _eth_reload_rx(eth, rx_id);
			/* Parse and set packet header data */
			/* FIXME: odp_packet_pull_tail(pkt, pkt_sock->max_frame_len - recv_bytes); */
			_odp_packet_reset_parse(pkt);

			pkt_table[nb_rx++] = pkt;
		}
	}
	return nb_rx;
}

static int eth_recv(pktio_entry_t *pktio_entry, odp_packet_t pkt_table[],
		    unsigned len)
{
	eth_status_t * eth = pktio_entry->s.pkt_eth.status;
	unsigned nb_rx;

	odp_spinlock_lock(&eth->rlock);
	INVALIDATE(eth);

	if (eth->refresh_rx)
		_eth_reload_rxes(eth);

	eth->bmask = mppa_noc_dnoc_rx_get_events_bitmask(eth->dma_if);

	nb_rx = _eth_poll_mask(eth, pkt_table, len);

	odp_spinlock_unlock(&eth->rlock);

	return nb_rx;
}

static int eth_send(pktio_entry_t *pktio_entry, odp_packet_t pkt_table[],
		    unsigned len)
{
	unsigned i;
	eth_status_t * eth = pktio_entry->s.pkt_eth.status;
	odp_spinlock_lock(&eth->wlock);
	odp_spinlock_unlock(&eth->wlock);

	for (i = 0; i < len; i++)
		odp_packet_free(pkt_table[i]);

	return len;
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
