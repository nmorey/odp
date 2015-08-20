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
#include "ucode_fw/ucode_eth.h"


#define MAX_ETH_SLOTS		2
#define MAX_ETH_PORTS		4
#define N_RX_PER_PORT		10
#define NOC_UC_COUNT		2
#define MAX_PKT_PER_UC		4
#define DNOC_CLUS_IFACE_ID	0

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
static unsigned int g_last_uc_used = 0;

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

extern char _heap_end;

static int cluster_init_dnoc_tx(void)
{
	int i;
	mppa_noc_ret_t ret;
	mppa_noc_dnoc_uc_configuration_t uc_conf = MPPA_NOC_DNOC_UC_CONFIGURATION_INIT;

	uc_conf.program_start = (uintptr_t) ucode_eth;
	uc_conf.buffer_base = (uintptr_t) &_data_start;
	uc_conf.buffer_size = (uintptr_t) &_heap_end - (uintptr_t) &_data_start;

	for (i = 0; i < NOC_UC_COUNT; i++) {

		/* DNoC */
		ret = mppa_noc_dnoc_tx_alloc_auto(DNOC_CLUS_IFACE_ID, &g_uc_ctx[i].dnoc_uc_id, MPPA_NOC_BLOCKING);
		if (ret != MPPA_NOC_RET_SUCCESS)
			return 1;

		/* We will only use events */
		mppa_noc_disable_interrupt_handler(DNOC_CLUS_IFACE_ID,
			MPPA_NOC_INTERRUPT_LINE_DNOC_TX, g_uc_ctx[i].dnoc_uc_id);

		ret = mppa_noc_dnoc_uc_alloc_auto(DNOC_CLUS_IFACE_ID, &g_uc_ctx[i].dnoc_uc_id, MPPA_NOC_BLOCKING);
		if (ret != MPPA_NOC_RET_SUCCESS)
			return 1;

		ret = mppa_noc_dnoc_uc_link(DNOC_CLUS_IFACE_ID, g_uc_ctx[i].dnoc_uc_id,
					    g_uc_ctx[i].dnoc_tx_id, uc_conf);
		if (ret != MPPA_NOC_RET_SUCCESS)
			return 1;
	}

	return 0;
}

static int eth_init(void)
{
	cluster_init_dnoc_tx();
	return 0;
}

typedef struct eth_status {
	odp_pool_t pool; 		 /**< pool to alloc packets from */
	odp_spinlock_t wlock;            /**< Tx lock */

	/* Rx Data */
	odp_spinlock_t rlock;            /**< Rx lock */
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

	/* Tx data */
	uint16_t tx_if;                  /**< Remote DMA interface to forward to Eth Egress */
	uint16_t tx_tag;                 /**< Remote DMA tag to forward to Eth Egress */
	mppa_dnoc_header_t header;
	mppa_dnoc_channel_config_t config;
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
	eth->config._.payload_min = 0;
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
			if (pkt == ODP_PACKET_INVALID)
				/* Packet was corrupted */
				continue;
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

	for(int i = eth->min_mask >> 1; i <= eth->max_mask >> 1 ; i += 1){
		*(uint64_t*)(&eth->bmask.bitmask[i]) = mppa_dnoc[eth->dma_if]->rx_global.events[i].dword;
	}

	nb_rx = _eth_poll_mask(eth, pkt_table, len);

	odp_spinlock_unlock(&eth->rlock);

	return nb_rx;
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

	/* Wait for previous run to complete */
	if (g_uc_ctx[g_last_uc_used].is_running) {
		mppa_noc_wait_clear_event(DNOC_CLUS_IFACE_ID,
					  MPPA_NOC_INTERRUPT_LINE_DNOC_TX,
					  g_uc_ctx[g_last_uc_used].dnoc_tx_id);
		/* Free previous packets */
		for(i = 0; i < pkt_count; i++)
			odp_packet_free(g_uc_ctx[g_last_uc_used].pkt_table[i]);
	}

	nret = mppa_noc_dnoc_uc_configure(DNOC_CLUS_IFACE_ID, g_uc_ctx[g_last_uc_used].dnoc_uc_id,
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
		g_uc_ctx[g_last_uc_used].pkt_table[i] = pkt_table[i];
	}

	uc_conf.pointers = &uc_pointers;
	uc_conf.event_counter = 0;

	g_uc_ctx[g_last_uc_used].pkt_count = pkt_count;
	g_uc_ctx[g_last_uc_used].is_running = 1;

	mppa_noc_dnoc_uc_set_program_run(DNOC_CLUS_IFACE_ID, g_uc_ctx[g_last_uc_used].dnoc_uc_id,
					 program_run);

	g_last_uc_used++;
	g_last_uc_used %= NOC_UC_COUNT;

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
