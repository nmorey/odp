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
#define MAX_JOB_PER_UC          32
#define DNOC_CLUS_IFACE_ID	0

#include <mppa_noc.h>
#include <mppa_routing.h>

extern char _heap_end;

typedef struct eth_uc_job_ctx {
	bool is_running;
	odp_packet_t pkt_table[MAX_PKT_PER_UC];
	unsigned int pkt_count;
} eth_uc_job_ctx_t;

typedef struct eth_uc_ctx {
	unsigned int dnoc_tx_id;
	unsigned int dnoc_uc_id;

	unsigned joined_jobs;
	unsigned int job_id;
	eth_uc_job_ctx_t job_ctxs[MAX_JOB_PER_UC];
} eth_uc_ctx_t;

static eth_uc_ctx_t g_uc_ctx[NOC_UC_COUNT] = {{0}};

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
						  &g_uc_ctx[i].dnoc_tx_id,
						  MPPA_NOC_BLOCKING);
		if (ret != MPPA_NOC_RET_SUCCESS)
			return 1;

		/* We will only use events */
		mppa_noc_disable_interrupt_handler(DNOC_CLUS_IFACE_ID,
						   MPPA_NOC_INTERRUPT_LINE_DNOC_TX,
						   g_uc_ctx[i].dnoc_tx_id);

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

static int eth_init(void)
{
	if (rx_thread_init())
		return 1;

	if(cluster_init_dnoc_tx())
		return 1;

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

	eth_status_t *eth = pktio_entry->s.pkt_eth.status;
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
	rx_thread_link_open(&eth->rx_config, N_RX_P_ETH);

	/*
	 * RPC Msg to IOETH  #N so the LB will dispatch to us
	 */
	odp_rpc_cmd_open_t open_cmd = {
		{
			.ifId = port_id,
			.dma_if = eth->rx_config.dma_if,
			.min_rx = eth->rx_config.min_port,
			.max_rx = eth->rx_config.max_port,
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

	/* Push Context to handling threads */
	rx_thread_link_close(slot_id * MAX_ETH_PORTS + port_id);

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

	qentry = queue_to_qentry(eth->rx_config.queue);
	return queue_deq_multi(qentry, (odp_buffer_hdr_t **)pkt_table, len);
}


static inline int
eth_send_packets(eth_status_t * eth, odp_packet_t pkt_table[], unsigned int pkt_count)
{
	mppa_noc_dnoc_uc_configuration_t uc_conf =
		MPPA_NOC_DNOC_UC_CONFIGURATION_INIT;
	mppa_noc_uc_program_run_t program_run = {{1, 1}};
	mppa_noc_ret_t nret;
	odp_packet_hdr_t * pkt_hdr;
	unsigned int i;
	mppa_noc_uc_pointer_configuration_t uc_pointers = {{0}};
	unsigned int tx_index = eth->port_id % NOC_UC_COUNT;

	for (i = 0; i < pkt_count; i++) {
		pkt_hdr = odp_packet_hdr(pkt_table[i]);
		/* Setup parameters and pointers */
		uc_conf.parameters[i * 2] = pkt_hdr->frame_len /
			sizeof(uint64_t);
		uc_conf.parameters[i * 2 + 1] = pkt_hdr->frame_len %
			sizeof(uint64_t);
		uc_pointers.thread_pointers[i] =
			(uintptr_t) packet_map(pkt_hdr, 0, NULL) -
			(uintptr_t) &_data_start;
	}
	for (i = pkt_count; i < MAX_PKT_PER_UC; i++) {
		uc_conf.parameters[i * 2] = 0;
		uc_conf.parameters[i * 2 + 1] = 0;
		uc_pointers.thread_pointers[i] = 0;
	}

	odp_spinlock_lock(&eth->wlock);
	INVALIDATE(g_uc_ctx);
	{
		eth_uc_ctx_t * ctx = &g_uc_ctx[tx_index];
		unsigned job_id = ctx->job_id++;
		eth_uc_job_ctx_t *job = &ctx->job_ctxs[job_id % MAX_JOB_PER_UC];

		/* Wait for previous run(s) to complete */
		while(ctx->joined_jobs + MAX_JOB_PER_UC <= job_id) {
			int ev_counter = mppa_noc_wait_clear_event(DNOC_CLUS_IFACE_ID,
								   MPPA_NOC_INTERRUPT_LINE_DNOC_TX,
								   ctx->dnoc_tx_id);
			for(i = ctx->joined_jobs; i < ctx->joined_jobs + ev_counter; ++i) {
				eth_uc_job_ctx_t * joined_job = &ctx->job_ctxs[i % MAX_JOB_PER_UC];
				joined_job->is_running = 0;
				/* Free previous packets */
				ret_buf(&((pool_entry_t *)eth->pool)->s,
					(odp_buffer_hdr_t**)joined_job->pkt_table, joined_job->pkt_count);
			}
			ctx->joined_jobs += ev_counter;
		}
		memcpy(job->pkt_table, pkt_table, pkt_count * sizeof(*pkt_table));

		uc_conf.pointers = &uc_pointers;
		uc_conf.event_counter = 0;

		nret = mppa_noc_dnoc_uc_configure(DNOC_CLUS_IFACE_ID, ctx->dnoc_uc_id,
						  uc_conf, eth->header, eth->config);
		if (nret != MPPA_NOC_RET_SUCCESS)
			return 1;

		job->pkt_count = pkt_count;
		job->is_running = 1;

		mppa_noc_dnoc_uc_set_program_run(DNOC_CLUS_IFACE_ID, ctx->dnoc_uc_id,
						 program_run);
	}
	odp_spinlock_unlock(&eth->wlock);

	return 0;
}

static int eth_send(pktio_entry_t *pktio_entry, odp_packet_t pkt_table[],
		    unsigned len)
{
	unsigned int sent = 0;
	eth_status_t * eth = pktio_entry->s.pkt_eth.status;
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
	return -1;
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
