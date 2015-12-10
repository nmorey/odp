/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */
#include <odp_packet_io_internal.h>
#include <odp/thread.h>
#include <odp/cpumask.h>
#include <HAL/hal/hal.h>
#include <odp/errno.h>
#include <errno.h>

#ifdef K1_NODEOS
#include <pthread.h>
#else
#include <utask.h>
#endif

#include "odp_pool_internal.h"
#include "odp_rpc_internal.h"
#include "odp_rx_internal.h"
#include "odp_tx_uc_internal.h"

#define MAX_ETH_SLOTS 2
#define MAX_ETH_PORTS 4
_ODP_STATIC_ASSERT(MAX_ETH_PORTS * MAX_ETH_SLOTS <= MAX_RX_ETH_IF,
		   "MAX_RX_ETH_IF__ERROR");

#define N_RX_P_ETH 12
#define NOC_ETH_UC_COUNT 2

#include <mppa_noc.h>
#include <mppa_routing.h>

#include "ucode_fw/ucode_eth.h"
#include "ucode_fw/ucode_eth_v2.h"

/**
 * #############################
 * PKTIO Interface
 * #############################
 */

static tx_uc_ctx_t g_eth_tx_uc_ctx[NOC_ETH_UC_COUNT] = {{0}};

static inline tx_uc_ctx_t *eth_get_ctx(const pkt_eth_t *eth)
{
	const unsigned int tx_index =
		eth->tx_config.config._.first_dir % NOC_ETH_UC_COUNT;
	return &g_eth_tx_uc_ctx[tx_index];
}

static int eth_init(void)
{
	if (rx_thread_init())
		return 1;

	return 0;
}

static int eth_destroy(void)
{
	/* Last pktio to close should work. Expect an err code for others */
	rx_thread_destroy();
	return 0;
}

static int eth_rpc_send_eth_open(odp_pktio_param_t * params, pkt_eth_t *eth)
{
	unsigned cluster_id = __k1_get_cluster_id();
	odp_rpc_t *ack_msg;
	odp_rpc_cmd_ack_t ack;
	int ret;

	/*
	 * RPC Msg to IOETH  #N so the LB will dispatch to us
	 */
	odp_rpc_cmd_eth_open_t open_cmd = {
		{
			.ifId = eth->port_id,
			.dma_if = eth->rx_config.dma_if,
			.min_rx = eth->rx_config.min_port,
			.max_rx = eth->rx_config.max_port,
			.loopback = eth->loopback,
			.rx_enabled = 1,
			.tx_enabled = 1,
		}
	};
	if (params) {
		if (params->in_mode == ODP_PKTIN_MODE_DISABLED)
			open_cmd.rx_enabled = 0;
		if (params->out_mode == ODP_PKTOUT_MODE_DISABLED)
			open_cmd.tx_enabled = 0;
	}
	odp_rpc_t cmd = {
		.data_len = 0,
		.pkt_type = ODP_RPC_CMD_ETH_OPEN,
		.inl_data = open_cmd.inl_data,
		.flags = 0,
	};

	odp_rpc_do_query(odp_rpc_get_ioeth_dma_id(eth->slot_id, cluster_id),
			 odp_rpc_get_ioeth_tag_id(eth->slot_id, cluster_id),
			 &cmd, NULL);

	ret = odp_rpc_wait_ack(&ack_msg, NULL, 15 * RPC_TIMEOUT_1S);
	if (ret < 0) {
		fprintf(stderr, "[ETH] RPC Error\n");
		return 1;
	} else if (ret == 0){
		fprintf(stderr, "[ETH] Query timed out\n");
		return 1;
	}

	ack.inl_data = ack_msg->inl_data;
	if (ack.status) {
		fprintf(stderr, "[ETH] Error: Server declined opening of eth interface\n");
		return 1;
	}

	eth->tx_if = ack.cmd.eth_open.tx_if;
	eth->tx_tag = ack.cmd.eth_open.tx_tag;
	memcpy(eth->mac_addr, ack.cmd.eth_open.mac, 6);
	eth->mtu = ack.cmd.eth_open.mtu;

	return 0;
}

static int eth_open(odp_pktio_t id ODP_UNUSED, pktio_entry_t *pktio_entry,
		    const char *devname, odp_pool_t pool)
{
	int ret = 0;
	int nRx = N_RX_P_ETH;
	int rr_policy = -1;
	int port_id, slot_id;
	int loopback = 0;
	int nofree = 0;
	/*
	 * Check device name and extract slot/port
	 */
	const char* pptr = devname;
	char * eptr;

	if (*(pptr++) != 'e')
		return -1;

	slot_id = strtoul(pptr, &eptr, 10);
	if (eptr == pptr || slot_id < 0 || slot_id >= MAX_ETH_SLOTS) {
		ODP_ERR("Invalid Ethernet name %s\n", devname);
		return -1;
	}

	pptr = eptr;
	if (*pptr == 'p') {
		/* Found a port */
		pptr++;
		port_id = strtoul(pptr, &eptr, 10);

		if (eptr == pptr || port_id < 0 || port_id >= MAX_ETH_PORTS) {
			ODP_ERR("Invalid Ethernet name %s\n", devname);
			return -1;
		}
		pptr = eptr;
	} else {
		/* Default port is 4 (40G), but physically lane 0 */
		port_id = 4;
	}

	while (*pptr == ':') {
		/* Parse arguments */
		pptr++;
		if (!strncmp(pptr, "tags=", strlen("tags="))){
			pptr += strlen("tags=");
			nRx = strtoul(pptr, &eptr, 10);
			if(pptr == eptr){
				ODP_ERR("Invalid tag count %s\n", pptr);
				return -1;
			}
			pptr = eptr;
		} else if (!strncmp(pptr, "rrpolicy=", strlen("rrpolicy="))){
			pptr += strlen("rrpolicy=");
			rr_policy = strtoul(pptr, &eptr, 10);
			if(pptr == eptr){
				ODP_ERR("Invalid rr_policy %s\n", pptr);
				return -1;
			}
			pptr = eptr;
		} else if (!strncmp(pptr, "loop", strlen("loop"))){
			pptr += strlen("loop");
			loopback = 1;
		} else if (!strncmp(pptr, "nofree", strlen("nofree"))){
			pptr += strlen("nofree");
			nofree = 1;
		} else {
			/* Unknown parameter */
			ODP_ERR("Invalid option %s\n", pptr);
			return -1;
		}
	}
	if (*pptr != 0) {
		/* Garbage at the end of the name... */
		ODP_ERR("Invalid option %s\n", pptr);
		return -1;
	}
#ifdef MAGIC_SCALL
	ODP_ERR("Trying to invoke ETH interface in simulation. Use magic: interface type");
	return 1;
#endif

	uintptr_t ucode;
#if MOS_UC_VERSION == 1
	ucode = (uintptr_t)ucode_eth;
#else
	ucode = (uintptr_t)ucode_eth_v2;
#endif

	pkt_eth_t *eth = &pktio_entry->s.pkt_eth;
	/*
	 * Init eth status
	 */
	eth->slot_id = slot_id;
	eth->port_id = port_id;
	eth->pool = pool;
	eth->loopback = loopback;
	eth->tx_config.nofree = nofree;
	eth->tx_config.add_end_marker = 0;

	if (pktio_entry->s.param.in_mode != ODP_PKTIN_MODE_DISABLED) {
		/* Setup Rx threads */
		eth->rx_config.dma_if = 0;
		eth->rx_config.pool = pool;
		eth->rx_config.pktio_id = slot_id * MAX_ETH_PORTS + port_id;
		eth->rx_config.header_sz = sizeof(mppa_ethernet_header_t);
		ret = rx_thread_link_open(&eth->rx_config, nRx, rr_policy);
		if(ret < 0)
			return -1;
	}

	ret = eth_rpc_send_eth_open(&pktio_entry->s.param, eth);

	if (pktio_entry->s.param.out_mode != ODP_PKTOUT_MODE_DISABLED) {
		tx_uc_init(g_eth_tx_uc_ctx, NOC_ETH_UC_COUNT, ucode, 0);

		mppa_routing_get_dnoc_unicast_route(__k1_get_cluster_id(),
						    eth->tx_if,
						    &eth->tx_config.config,
						    &eth->tx_config.header);

		eth->tx_config.config._.loopback_multicast = 0;
		eth->tx_config.config._.cfg_pe_en = 1;
		eth->tx_config.config._.cfg_user_en = 1;
		eth->tx_config.config._.write_pe_en = 1;
		eth->tx_config.config._.write_user_en = 1;
		eth->tx_config.config._.decounter_id = 0;
		eth->tx_config.config._.decounted = 0;
		eth->tx_config.config._.payload_min = 6;
		eth->tx_config.config._.payload_max = 32;
		eth->tx_config.config._.bw_current_credit = 0xff;
		eth->tx_config.config._.bw_max_credit     = 0xff;
		eth->tx_config.config._.bw_fast_delay     = 0x00;
		eth->tx_config.config._.bw_slow_delay     = 0x00;

		eth->tx_config.header._.multicast = 0;
		eth->tx_config.header._.tag = eth->tx_tag;
		eth->tx_config.header._.valid = 1;
	}

	return ret;
}

static int eth_close(pktio_entry_t * const pktio_entry)
{

	pkt_eth_t *eth = &pktio_entry->s.pkt_eth;
	int slot_id = eth->slot_id;
	int port_id = eth->port_id;
	odp_rpc_t *ack_msg;
	odp_rpc_cmd_ack_t ack;
	int ret;
	odp_rpc_cmd_eth_clos_t close_cmd = {
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

	/* Free packets being sent by DMA */
	tx_uc_flush(eth_get_ctx(eth));

	odp_rpc_do_query(odp_rpc_get_ioeth_dma_id(slot_id, cluster_id),
			 odp_rpc_get_ioeth_tag_id(slot_id, cluster_id),
			 &cmd, NULL);

	ret = odp_rpc_wait_ack(&ack_msg, NULL, 5 * RPC_TIMEOUT_1S);
	if (ret < 0) {
		fprintf(stderr, "[ETH] RPC Error\n");
		return 1;
	} else if (ret == 0){
		fprintf(stderr, "[ETH] Query timed out\n");
		return 1;
	}
	ack.inl_data = ack_msg->inl_data;

	/* Push Context to handling threads */
	rx_thread_link_close(slot_id * MAX_ETH_PORTS + port_id);

	return ack.status;
}

static int eth_mac_addr_get(pktio_entry_t *pktio_entry,
			    void *mac_addr)
{
	pkt_eth_t *eth = &pktio_entry->s.pkt_eth;
	memcpy(mac_addr, eth->mac_addr, ETH_ALEN);
	return ETH_ALEN;
}




static int eth_recv(pktio_entry_t *pktio_entry, odp_packet_t pkt_table[],
		    unsigned len)
{
	int n_packet;
	pkt_eth_t *eth = &pktio_entry->s.pkt_eth;

	n_packet = odp_buffer_ring_get_multi(eth->rx_config.ring,
					     (odp_buffer_hdr_t **)pkt_table,
					     len, NULL);

	for (int i = 0; i < n_packet; ++i) {
		odp_packet_t pkt = pkt_table[i];
		odp_packet_hdr_t *pkt_hdr = odp_packet_hdr(pkt);
		uint8_t * const base_addr =
			((uint8_t *)pkt_hdr->buf_hdr.addr) +
			pkt_hdr->headroom;

		packet_parse_reset(pkt);

		union mppa_ethernet_header_info_t info;
		uint8_t * const hdr_addr = base_addr -
			sizeof(mppa_ethernet_header_t);
		mppa_ethernet_header_t * const header =
			(mppa_ethernet_header_t *)hdr_addr;

		info.dword = LOAD_U64(header->info.dword);
		const unsigned frame_len =
			info._.pkt_size - sizeof(mppa_ethernet_header_t);
		pull_tail(pkt_hdr, pkt_hdr->frame_len - frame_len);
	}
	return n_packet;
}

static int eth_send(pktio_entry_t *pktio_entry, odp_packet_t pkt_table[],
		    unsigned len)
{
	pkt_eth_t *eth = &pktio_entry->s.pkt_eth;
	tx_uc_ctx_t *ctx = eth_get_ctx(eth);

	return tx_uc_send_packets(&eth->tx_config, ctx,
				  pkt_table, len,
				  eth->mtu);
}

static int eth_promisc_mode_set(pktio_entry_t *const pktio_entry,
				odp_bool_t enable){
	/* FIXME */
	pktio_entry->s.pkt_eth.promisc = enable;
	return 0;
}

static int eth_promisc_mode(pktio_entry_t *const pktio_entry){
	return 	pktio_entry->s.pkt_eth.promisc;
}

static int eth_mtu_get(pktio_entry_t *const pktio_entry) {
	pkt_eth_t *eth = &pktio_entry->s.pkt_eth;
	return eth->mtu;
}
const pktio_if_ops_t eth_pktio_ops = {
	.init = eth_init,
	.term = eth_destroy,
	.open = eth_open,
	.close = eth_close,
	.start = NULL,
	.stop = NULL,
	.recv = eth_recv,
	.send = eth_send,
	.mtu_get = eth_mtu_get,
	.promisc_mode_set = eth_promisc_mode_set,
	.promisc_mode_get = eth_promisc_mode,
	.mac_get = eth_mac_addr_get,
};
