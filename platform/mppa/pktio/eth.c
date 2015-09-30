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
#include "odp_rx_internal.h"
#include "ucode_fw/ucode_eth.h"


#define MAX_ETH_SLOTS 2
#define MAX_ETH_PORTS 4
#define N_RX_P_ETH 12

#define NOC_UC_COUNT		2
#define MAX_PKT_PER_UC		4
/* must be > greater than max_threads */
#define MAX_JOB_PER_UC          MOS_NB_UC_TRS
#define DNOC_CLUS_IFACE_ID	0

#include <mppa_noc.h>
#include <mppa_routing.h>

extern char _heap_end;

typedef struct eth_uc_job_ctx {
	odp_packet_t pkt_table[MAX_PKT_PER_UC];
	unsigned int pkt_count;
} eth_uc_job_ctx_t;

typedef struct eth_uc_ctx {
	unsigned int dnoc_tx_id;
	unsigned int dnoc_uc_id;

	unsigned joined_jobs;
	odp_atomic_u64_t prepar_head;
	odp_atomic_u64_t commit_head;
	eth_uc_job_ctx_t job_ctxs[MAX_JOB_PER_UC];
} eth_uc_ctx_t;

static eth_uc_ctx_t g_eth_uc_ctx[NOC_UC_COUNT] = {{0}};

typedef struct eth_status {
	odp_pool_t pool;                      /**< pool to alloc packets from */
	odp_spinlock_t wlock;        /**< Tx lock */

	/* Rx Data */
	rx_config_t rx_config;

	uint8_t slot_id;             /**< IO Eth Id */
	uint8_t port_id;             /**< Eth Port id. 4 for 40G */

	/* Tx data */
	uint16_t tx_if;              /**< Remote DMA interface to forward
				      *   to Eth Egress */
	uint16_t tx_tag;             /**< Remote DMA tag to forward to
				      *   Eth Egress */

	mppa_dnoc_header_t header;
	mppa_dnoc_channel_config_t config;

} eth_status_t;

/**
 * #############################
 * PKTIO Interface
 * #############################
 */

static int eth_init_dnoc_tx(void)
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
						  &g_eth_uc_ctx[i].dnoc_tx_id,
						  MPPA_NOC_BLOCKING);
		if (ret != MPPA_NOC_RET_SUCCESS)
			return 1;

		ret = mppa_noc_dnoc_uc_alloc_auto(DNOC_CLUS_IFACE_ID,
						  &g_eth_uc_ctx[i].dnoc_uc_id,
						  MPPA_NOC_BLOCKING);
		if (ret != MPPA_NOC_RET_SUCCESS)
			return 1;

		/* We will only use events */
		mppa_noc_disable_interrupt_handler(DNOC_CLUS_IFACE_ID,
						   MPPA_NOC_INTERRUPT_LINE_DNOC_TX,
						   g_eth_uc_ctx[i].dnoc_uc_id);


		ret = mppa_noc_dnoc_uc_link(DNOC_CLUS_IFACE_ID,
					    g_eth_uc_ctx[i].dnoc_uc_id,
					    g_eth_uc_ctx[i].dnoc_tx_id, uc_conf);
		if (ret != MPPA_NOC_RET_SUCCESS)
			return 1;
	}

	return 0;
}

static int eth_init(void)
{
	if (rx_thread_init())
		return 1;

	if(eth_init_dnoc_tx())
		return 1;

	return 0;
}

static int eth_rpc_send_eth_open(pkt_eth_t *eth)
{
	unsigned cluster_id = __k1_get_cluster_id();
	odp_rpc_t *ack_msg;
	odp_rpc_cmd_ack_t ack;

	/*
	 * RPC Msg to IOETH  #N so the LB will dispatch to us
	 */
	odp_rpc_cmd_eth_open_t open_cmd = {
		{
			.ifId = eth->port_id,
			.dma_if = eth->rx_config.dma_if,
			.min_rx = eth->rx_config.min_port,
			.max_rx = eth->rx_config.max_port,
		}
	};
	odp_rpc_t cmd = {
		.data_len = 0,
		.pkt_type = ODP_RPC_CMD_ETH_OPEN,
		.inl_data = open_cmd.inl_data,
		.flags = 0,
	};

	odp_rpc_do_query(odp_rpc_get_ioeth_dma_id(eth->slot_id, cluster_id),
			 odp_rpc_get_ioeth_tag_id(eth->slot_id, cluster_id),
			 &cmd, NULL);

	odp_rpc_wait_ack(&ack_msg, NULL);
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

	pkt_eth_t *eth = &pktio_entry->s.pkt_eth;
	/*
	 * Init eth status
	 */
	eth->slot_id = slot_id;
	eth->port_id = port_id;
	eth->pool = pool;
	odp_spinlock_init(&eth->wlock);

	/* Setup Rx threads */
	eth->rx_config.dma_if = 0;
	eth->rx_config.pool = pool;
	eth->rx_config.pktio_id = slot_id * MAX_ETH_PORTS + port_id;
	eth->rx_config.header_sz = sizeof(mppa_ethernet_header_t);
	ret = rx_thread_link_open(&eth->rx_config, nRx, rr_policy);
	if(ret < 0)
		return -1;

	ret = eth_rpc_send_eth_open(eth);

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

	return ret;
}

static int eth_close(pktio_entry_t * const pktio_entry)
{

	pkt_eth_t *eth = &pktio_entry->s.pkt_eth;
	int slot_id = eth->slot_id;
	int port_id = eth->port_id;
	odp_rpc_t *ack_msg;
	odp_rpc_cmd_ack_t ack;

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

	odp_rpc_do_query(odp_rpc_get_ioeth_dma_id(slot_id, cluster_id),
			 odp_rpc_get_ioeth_tag_id(slot_id, cluster_id),
			 &cmd, NULL);

	odp_rpc_wait_ack(&ack_msg, NULL);
	ack.inl_data = ack_msg->inl_data;

	/* Push Context to handling threads */
	rx_thread_link_close(slot_id * MAX_ETH_PORTS + port_id);

	free(eth);
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
	queue_entry_t *qentry;

	qentry = queue_to_qentry(eth->rx_config.queue);
	n_packet =
		queue_deq_multi(qentry, (odp_buffer_hdr_t **)pkt_table, len);

	for (int i = 0; i < n_packet; ++i) {
		odp_packet_t pkt = pkt_table[i];
		odp_packet_hdr_t *pkt_hdr = odp_packet_hdr(pkt);
		uint8_t * const base_addr =
			((uint8_t *)pkt_hdr->buf_hdr.addr) +
			pkt_hdr->headroom;

		_odp_packet_reset_parse(pkt);

		union mppa_ethernet_header_info_t info;
		uint8_t * const hdr_addr = base_addr -
			sizeof(mppa_ethernet_header_t);
		mppa_ethernet_header_t * const header =
			(mppa_ethernet_header_t *)hdr_addr;

		info.dword = LOAD_U64(header->info.dword);
		packet_set_len(pkt, info._.pkt_size -
			       sizeof(mppa_ethernet_header_t));
	}
	return n_packet;
}


static inline int
eth_send_packets(pkt_eth_t *eth, odp_packet_t pkt_table[], unsigned int pkt_count)
{
	const unsigned int tx_index = eth->port_id % NOC_UC_COUNT;
	const eth_uc_ctx_t * ctx = &g_eth_uc_ctx[tx_index];
	const uint64_t prepar_id = odp_atomic_fetch_inc_u64(&g_eth_uc_ctx[tx_index].prepar_head);
	unsigned  ev_counter, diff;

	/* Wait for slot */
	ev_counter = mOS_uc_read_event(ctx->dnoc_uc_id);
	diff = ((uint32_t)prepar_id) - ev_counter;
	while (diff > 0x80000000 || ev_counter + MAX_JOB_PER_UC <= ((uint32_t)prepar_id)) {
		odp_spin();
		ev_counter = mOS_uc_read_event(ctx->dnoc_uc_id);
		diff = ((uint32_t)prepar_id) - ev_counter;
	}

	const unsigned slot_id = prepar_id % MAX_JOB_PER_UC;
	eth_uc_job_ctx_t * job = &g_eth_uc_ctx[tx_index].job_ctxs[slot_id];

	if(prepar_id > MAX_JOB_PER_UC){
		/* Free previous packets */
		ret_buf(&((pool_entry_t *)eth->pool)->s,
			(odp_buffer_hdr_t**)job->pkt_table, job->pkt_count);
	}

	mOS_uc_transaction_t * const trs =  &_scoreboard_start.SCB_UC.trs [ctx->dnoc_uc_id][slot_id];
	const odp_packet_hdr_t * pkt_hdr;

	for (unsigned i = 0; i < pkt_count; ++i ){
		job->pkt_table[i] = pkt_table[i];
		pkt_hdr = odp_packet_hdr(pkt_table[i]);

		trs->parameter.array[2 * i + 0] =
			pkt_hdr->frame_len / sizeof(uint64_t);
		trs->parameter.array[2 * i + 1] =
			pkt_hdr->frame_len % sizeof(uint64_t);

		trs->pointer.array[i] = (unsigned long)
			(((uint8_t*)pkt_hdr->buf_hdr.addr + pkt_hdr->headroom)
			 - (uint8_t*)&_data_start);
	}
	for (unsigned i = pkt_count; i < 4; ++i) {
		trs->parameter.array[2 * i + 0] = 0;
		trs->parameter.array[2 * i + 1] = 0;
	}

	trs->path.array[ctx->dnoc_tx_id].header = eth->header;
	trs->path.array[ctx->dnoc_tx_id].config = eth->config;
	trs->notify._word = 0;
	trs->desc.tx_set = 1 << ctx->dnoc_tx_id;
	trs->desc.param_set = 0xff;
	trs->desc.pointer_set = (0x1 <<  pkt_count) - 1;

	job->pkt_count = pkt_count;


	while (odp_atomic_load_u64(&g_eth_uc_ctx[tx_index].commit_head) != prepar_id)
		odp_spin();


	__builtin_k1_wpurge();
	__builtin_k1_fence ();
	mOS_ucore_commit(ctx->dnoc_tx_id);
	odp_atomic_inc_u64(&g_eth_uc_ctx[tx_index].commit_head);

	return 0;
}

static int eth_send(pktio_entry_t *pktio_entry, odp_packet_t pkt_table[],
		    unsigned len)
{
	unsigned int sent = 0;
	pkt_eth_t *eth = &pktio_entry->s.pkt_eth;
	unsigned int pkt_count;


	while(sent < len) {
		pkt_count = (len - sent) > MAX_PKT_PER_UC ? MAX_PKT_PER_UC :
			(len - sent);

		eth_send_packets(eth, &pkt_table[sent], pkt_count);
		sent += pkt_count;
	}


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
	pkt_eth_t *eth = &pktio_entry->s.pkt_eth;
	return eth->mtu;
}
const pktio_if_ops_t eth_pktio_ops = {
	.init = eth_init,
	.term = NULL,
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
