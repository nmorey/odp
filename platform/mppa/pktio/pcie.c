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
#include "ucode_fw/ucode_pcie.h"


#define MAX_PCIE_SLOTS 2
#define MAX_PCIE_INTERFACES 4
_ODP_STATIC_ASSERT(MAX_PCIE_INTERFACES * MAX_PCIE_SLOTS <= MAX_RX_PCIE_IF,
		   "MAX_RX_PCIE_IF__ERROR");

#define N_RX_P_PCIE 12

#define NOC_PCIE_UC_COUNT	2
#define DNOC_CLUS_IFACE_ID	0

#define PKTIO_PKT_MTU	1500

#include <mppa_noc.h>
#include <mppa_routing.h>

/**
 * #############################
 * PKTIO Interface
 * #############################
 */

static tx_uc_ctx_t g_pcie_tx_uc_ctx[NOC_PCIE_UC_COUNT] = {{0}};

static inline tx_uc_ctx_t *pcie_get_ctx(const pkt_pcie_t *pcie)
{
	const unsigned int tx_index =
		pcie->tx_config.config._.first_dir % NOC_PCIE_UC_COUNT;
	return &g_pcie_tx_uc_ctx[tx_index];
}

static int pcie_init(void)
{
	if (rx_thread_init())
		return 1;

	return 0;
}

static int pcie_destroy(void)
{
	/* Last pktio to close should work. Expect an err code for others */
	rx_thread_destroy();
	return 0;
}

static int pcie_rpc_send_pcie_open(pkt_pcie_t *pcie)
{
	unsigned cluster_id = __k1_get_cluster_id();
	odp_rpc_t *ack_msg;
	odp_rpc_cmd_ack_t ack;
	int ret;
	/*
	 * RPC Msg to IOPCIE  #N so the LB will dispatch to us
	 */
	odp_rpc_cmd_pcie_open_t open_cmd = {
		{
			.pcie_eth_if_id = pcie->pcie_eth_if_id,
			.pkt_size = PKTIO_PKT_MTU,
			.min_rx = pcie->rx_config.min_port,
			.max_rx = pcie->rx_config.max_port,
		}
	};
	odp_rpc_t cmd = {
		.data_len = 0,
		.pkt_type = ODP_RPC_CMD_PCIE_OPEN,
		.inl_data = open_cmd.inl_data,
		.flags = 0,
	};

	odp_rpc_do_query(odp_rpc_get_ioddr_dma_id(pcie->slot_id, cluster_id),
			 odp_rpc_get_ioddr_tag_id(pcie->slot_id, cluster_id),
			 &cmd, NULL);

	ret = odp_rpc_wait_ack(&ack_msg, NULL, 15 * RPC_TIMEOUT_1S);
	if (ret < 0) {
		fprintf(stderr, "[PCIE] RPC Error\n");
		return 1;
	} else if (ret == 0){
		fprintf(stderr, "[PCIE] Query timed out\n");
		return 1;
	}
	ack.inl_data = ack_msg->inl_data;
	if (ack.status) {
		fprintf(stderr, "[PCIE] Error: Server declined opening of pcie interface\n");
		return 1;
	}

	pcie->tx_if = ack.cmd.pcie_open.tx_if;
	pcie->tx_tag = ack.cmd.pcie_open.tx_tag;
	memcpy(pcie->mac_addr, ack.cmd.pcie_open.mac, ETH_ALEN);
	pcie->mtu = ack.cmd.pcie_open.mtu;

	return 0;
}

static int pcie_open(odp_pktio_t id ODP_UNUSED, pktio_entry_t *pktio_entry,
		    const char *devname, odp_pool_t pool)
{
	int ret = 0;
	int nRx = N_RX_P_PCIE;
	int rr_policy = -1;
	int port_id, slot_id;
	int nofree = 0;

	/*
	 * Check device name and extract slot/port
	 */
	const char* pptr = devname;
	char * eptr;

	if (*(pptr++) != 'p')
		return -1;

	slot_id = strtoul(pptr, &eptr, 10);
	if (eptr == pptr || slot_id < 0 || slot_id >= MAX_PCIE_SLOTS) {
		ODP_ERR("Invalid PCIE name %s\n", devname);
		return -1;
	}

	pptr = eptr;
	if (*pptr == 'p') {
		/* Found a port */
		pptr++;
		port_id = strtoul(pptr, &eptr, 10);

		if (eptr == pptr || port_id < 0 || port_id >= MAX_PCIE_INTERFACES) {
			ODP_ERR("Invalid PCIE name %s\n", devname);
			return -1;
		}
		pptr = eptr;
	} else {
		ODP_ERR("Invalid PCIE name %s. Missing port number\n", devname);
		return -1;
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
	ODP_ERR("Trying to invoke PCIE interface in simulation. Use magic: interface type");
	return 1;
#endif


	uintptr_t ucode;
#if MOS_UC_VERSION == 1
	ucode = (uintptr_t)ucode_pcie;
#else
	ucode = (uintptr_t)ucode_pcie_v2;
#endif


	pkt_pcie_t *pcie = &pktio_entry->s.pkt_pcie;

	pcie->slot_id = slot_id;
	pcie->pcie_eth_if_id = port_id;
	pcie->pool = pool;
	odp_spinlock_init(&pcie->wlock);
	pcie->tx_config.nofree = nofree;
	pcie->tx_config.add_end_marker = 1;

	/* Setup Rx threads */
	if (pktio_entry->s.param.in_mode != ODP_PKTIN_MODE_DISABLED) {
		pcie->rx_config.dma_if = 0;
		pcie->rx_config.pool = pool;
		pcie->rx_config.pktio_id = slot_id * MAX_PCIE_INTERFACES + port_id +
			MAX_RX_ETH_IF;
		/* FIXME */
		pcie->rx_config.header_sz = sizeof(uint32_t);
		rx_thread_link_open(&pcie->rx_config, nRx, rr_policy);
	}

	ret = pcie_rpc_send_pcie_open(pcie);

	if (pktio_entry->s.param.out_mode != ODP_PKTOUT_MODE_DISABLED) {
		tx_uc_init(g_pcie_tx_uc_ctx, NOC_PCIE_UC_COUNT, ucode, 1);

		mppa_routing_get_dnoc_unicast_route(__k1_get_cluster_id(),
						    pcie->tx_if,
						    &pcie->tx_config.config,
						    &pcie->tx_config.header);

		pcie->tx_config.config._.loopback_multicast = 0;
		pcie->tx_config.config._.cfg_pe_en = 1;
		pcie->tx_config.config._.cfg_user_en = 1;
		pcie->tx_config.config._.write_pe_en = 1;
		pcie->tx_config.config._.write_user_en = 1;
		pcie->tx_config.config._.decounter_id = 0;
		pcie->tx_config.config._.decounted = 0;
		pcie->tx_config.config._.payload_min = 1;
		pcie->tx_config.config._.payload_max = 32;
		pcie->tx_config.config._.bw_current_credit = 0xff;
		pcie->tx_config.config._.bw_max_credit     = 0xff;
		pcie->tx_config.config._.bw_fast_delay     = 0x00;
		pcie->tx_config.config._.bw_slow_delay     = 0x00;

		pcie->tx_config.header._.multicast = 0;
		pcie->tx_config.header._.tag = pcie->tx_tag;
		pcie->tx_config.header._.valid = 1;
	}

	return ret;
}

static int pcie_close(pktio_entry_t * const pktio_entry)
{

	pkt_pcie_t *pcie = &pktio_entry->s.pkt_pcie;
	int slot_id = pcie->slot_id;
	int pcie_eth_if_id = pcie->pcie_eth_if_id;
	odp_rpc_t *ack_msg;
	odp_rpc_cmd_ack_t ack;
	int ret;
	odp_rpc_cmd_pcie_clos_t close_cmd = {
		{
			.ifId = pcie->pcie_eth_if_id = pcie_eth_if_id

		}
	};
	unsigned cluster_id = __k1_get_cluster_id();
	odp_rpc_t cmd = {
		.pkt_type = ODP_RPC_CMD_PCIE_CLOS,
		.data_len = 0,
		.flags = 0,
		.inl_data = close_cmd.inl_data
	};

	/* Free packets being sent by DMA */
	tx_uc_flush(pcie_get_ctx(pcie));

	odp_rpc_do_query(odp_rpc_get_ioddr_dma_id(slot_id, cluster_id),
			 odp_rpc_get_ioddr_tag_id(slot_id, cluster_id),
			 &cmd, NULL);

	ret = odp_rpc_wait_ack(&ack_msg, NULL, 5 * RPC_TIMEOUT_1S);
	if (ret < 0) {
		fprintf(stderr, "[PCIE] RPC Error\n");
		return 1;
	} else if (ret == 0){
		fprintf(stderr, "[PCIE] Query timed out\n");
		return 1;
	}
	ack.inl_data = ack_msg->inl_data;

	/* Push Context to handling threads */
	rx_thread_link_close(slot_id * MAX_PCIE_INTERFACES + pcie_eth_if_id);

	return ack.status;
}

static int pcie_mac_addr_get(pktio_entry_t *pktio_entry ODP_UNUSED,
			    void *mac_addr ODP_UNUSED)
{
	pkt_pcie_t *pcie = &pktio_entry->s.pkt_pcie;
	memcpy(mac_addr, pcie->mac_addr, ETH_ALEN);
	return ETH_ALEN;
}

static int pcie_recv(pktio_entry_t *pktio_entry, odp_packet_t pkt_table[],
		    unsigned len)
{
	int n_packet;
	pkt_pcie_t *pcie = &pktio_entry->s.pkt_pcie;

	n_packet = odp_buffer_ring_get_multi(pcie->rx_config.ring,
					     (odp_buffer_hdr_t **)pkt_table,
					     len, NULL);

	for (int i = 0; i < n_packet; ++i) {
		odp_packet_t pkt = pkt_table[i];
		odp_packet_hdr_t *pkt_hdr = odp_packet_hdr(pkt);

		uint8_t * const base_addr =
			((uint8_t *)pkt_hdr->buf_hdr.addr) +
			pkt_hdr->headroom;

		packet_parse_reset(pkt);

		uint32_t size;
		uint8_t * const hdr_addr = base_addr -
			sizeof(uint32_t);

		size = __builtin_k1_lwu(hdr_addr);
		memset(hdr_addr, 0xab, 4);
		packet_set_len(pkt, size);
	}
	__k1_wmb();
	return n_packet;
}


static int pcie_send(pktio_entry_t *pktio_entry, odp_packet_t pkt_table[],
		    unsigned len)
{
	pkt_pcie_t *pcie = &pktio_entry->s.pkt_pcie;
	tx_uc_ctx_t *ctx = pcie_get_ctx(pcie);

	return tx_uc_send_aligned_packets(&pcie->tx_config, ctx,
					  pkt_table, len, PKTIO_PKT_MTU);
}

static int pcie_promisc_mode_set(pktio_entry_t *const pktio_entry ODP_UNUSED,
				odp_bool_t enable ODP_UNUSED){
	return -1;
}

static int pcie_promisc_mode(pktio_entry_t *const pktio_entry ODP_UNUSED){
	return -1;
}

static int pcie_mtu_get(pktio_entry_t *const pktio_entry ODP_UNUSED) {
	pkt_eth_t *eth = &pktio_entry->s.pkt_eth;
	return eth->mtu;
}
const pktio_if_ops_t pcie_pktio_ops = {
	.init = pcie_init,
	.term = pcie_destroy,
	.open = pcie_open,
	.close = pcie_close,
	.start = NULL,
	.stop = NULL,
	.recv = pcie_recv,
	.send = pcie_send,
	.mtu_get = pcie_mtu_get,
	.promisc_mode_set = pcie_promisc_mode_set,
	.promisc_mode_get = pcie_promisc_mode,
	.mac_get = pcie_mac_addr_get,
};
