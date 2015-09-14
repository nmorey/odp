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
#include "ucode_fw/ucode_pcie.h"


#define MAX_PCIE_SLOTS 2
#define MAX_PCIE_INTERFACES 4
#define N_RX_P_PCIE 12

#define NOC_UC_COUNT		2
#define MAX_PKT_PER_UC		8
/* must be > greater than max_threads */
#define MAX_JOB_PER_UC          32
#define DNOC_CLUS_IFACE_ID	0

#include <mppa_noc.h>
#include <mppa_routing.h>

extern char _heap_end;

typedef struct pcie_uc_job_ctx {
	bool is_running;
	odp_packet_t pkt_table[MAX_PKT_PER_UC];
	unsigned int pkt_count;
} pcie_uc_job_ctx_t;

typedef struct pcie_uc_ctx {
	unsigned int dnoc_tx_id;
	unsigned int dnoc_uc_id;

	unsigned joined_jobs;
	unsigned int job_id;
	pcie_uc_job_ctx_t job_ctxs[MAX_JOB_PER_UC];
} pcie_uc_ctx_t;

static pcie_uc_ctx_t g_uc_ctx[NOC_UC_COUNT] = {{0}};

typedef struct pcie_status {
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

} pcie_status_t;

/**
 * #############################
 * PKTIO Interface
 * #############################
 */

static int pcie_init_dnoc_tx(void)
{
	int i;
	mppa_noc_ret_t ret;
	mppa_noc_dnoc_uc_configuration_t uc_conf =
		MPPA_NOC_DNOC_UC_CONFIGURATION_INIT;

	uc_conf.program_start = (uintptr_t)ucode_pcie;
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

static int pcie_init(void)
{
	if (rx_thread_init())
		return 1;

	if(pcie_init_dnoc_tx())
		return 1;

	return 0;
}

static int pcie_rpc_send_pcie_open(pcie_status_t *pcie)
{
	unsigned cluster_id = __k1_get_cluster_id();
	odp_rpc_t *ack_msg;
	odp_rpc_cmd_ack_t ack;

	/*
	 * RPC Msg to IOPCIE  #N so the LB will dispatch to us
	 */
	odp_rpc_cmd_pcie_open_t open_cmd = {
		{
			.ifId = pcie->port_id,
			.dma_if = pcie->rx_config.dma_if,
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

	odp_rpc_wait_ack(&ack_msg, NULL);
	ack.inl_data = ack_msg->inl_data;
	if (ack.status) {
		fprintf(stderr, "[PCIE] Error: Server declined opening of pcie interface\n");
		return 1;
	}

	pcie->tx_if = ack.cmd.pcie_open.tx_if;
	pcie->tx_tag = ack.cmd.pcie_open.tx_tag;

	return 0;
}

static int pcie_open(odp_pktio_t id ODP_UNUSED, pktio_entry_t *pktio_entry,
		    const char *devname, odp_pool_t pool)
{
	int ret = 0;
	/*
	 * Check device name and extract slot/port
	 */
	if (devname[0] != 'p')
		return -1;

	int slot_id = devname[1] - '0';
	if (slot_id < 0 || slot_id >= MAX_PCIE_SLOTS)
		return -1;

	int port_id = 4;
	if (devname[2] != 0) {
		if(devname[2] != 'p')
			return -1;
		port_id = devname[3] - '0';

		if (port_id < 0 || port_id >= MAX_PCIE_INTERFACES)
			return -1;

		if(devname[4] != 0)
			return -1;
	}

	pktio_entry->s.pkt_data = malloc(sizeof(pcie_status_t));
	if (!pktio_entry->s.pkt_data)
		return -1;

	pcie_status_t *pcie = pktio_entry->s.pkt_data;

	pcie->slot_id = slot_id;
	pcie->port_id = port_id;
	pcie->pool = pool;
	odp_spinlock_init(&pcie->wlock);

	/* Setup Rx threads */
	pcie->rx_config.dma_if = 0;
	pcie->rx_config.pool = pool;
	pcie->rx_config.pktio_id = slot_id * MAX_PCIE_INTERFACES + port_id;
	/* FIXME */
	pcie->rx_config.header_sz = sizeof(NULL);
	rx_thread_link_open(&pcie->rx_config, N_RX_P_PCIE);

	ret = pcie_rpc_send_pcie_open(pcie);

	mppa_routing_get_dnoc_unicast_route(__k1_get_cluster_id(), pcie->tx_if,
					    &pcie->config, &pcie->header);

	pcie->config._.loopback_multicast = 0;
	pcie->config._.cfg_pe_en = 1;
	pcie->config._.cfg_user_en = 1;
	pcie->config._.write_pe_en = 1;
	pcie->config._.write_user_en = 1;
	pcie->config._.decounter_id = 0;
	pcie->config._.decounted = 0;
	pcie->config._.payload_min = 1;
	pcie->config._.payload_max = 32;
	pcie->config._.bw_current_credit = 0xff;
	pcie->config._.bw_max_credit     = 0xff;
	pcie->config._.bw_fast_delay     = 0x00;
	pcie->config._.bw_slow_delay     = 0x00;

	pcie->header._.tag = pcie->tx_tag;
	pcie->header._.valid = 1;

	return ret;
}

static int pcie_close(pktio_entry_t * const pktio_entry)
{

	pcie_status_t * pcie = pktio_entry->s.pkt_data;
	int slot_id = pcie->slot_id;
	int port_id = pcie->port_id;
	odp_rpc_t *ack_msg;
	odp_rpc_cmd_ack_t ack;

	odp_rpc_cmd_clos_t close_cmd = {
		{
			.ifId = pcie->port_id = port_id

		}
	};
	unsigned cluster_id = __k1_get_cluster_id();
	odp_rpc_t cmd = {
		.pkt_type = ODP_RPC_CMD_PCIE_CLOS,
		.data_len = 0,
		.flags = 0,
		.inl_data = close_cmd.inl_data
	};

	odp_rpc_do_query(odp_rpc_get_ioddr_dma_id(slot_id, cluster_id),
			 odp_rpc_get_ioddr_tag_id(slot_id, cluster_id),
			 &cmd, NULL);

	odp_rpc_wait_ack(&ack_msg, NULL);
	ack.inl_data = ack_msg->inl_data;

	/* Push Context to handling threads */
	rx_thread_link_close(slot_id * MAX_PCIE_INTERFACES + port_id);

	free(pcie);
	pktio_entry->s.pkt_data = NULL;
	return ack.status;
}

static int pcie_mac_addr_get(pktio_entry_t *pktio_entry ODP_UNUSED,
			    void *mac_addr ODP_UNUSED)
{
	/* FIXME */
	return -1;
}

static int pcie_recv(pktio_entry_t *pktio_entry, odp_packet_t pkt_table[],
		    unsigned len)
{
	pcie_status_t * pcie = pktio_entry->s.pkt_data;
	queue_entry_t *qentry;

	qentry = queue_to_qentry(pcie->rx_config.queue);
	return queue_deq_multi(qentry, (odp_buffer_hdr_t **)pkt_table, len);
}

static inline int
pcie_send_packets(pcie_status_t * pcie, odp_packet_t pkt_table[], unsigned int pkt_count)
{
	mppa_noc_dnoc_uc_configuration_t uc_conf =
		MPPA_NOC_DNOC_UC_CONFIGURATION_INIT;
	mppa_noc_uc_program_run_t program_run = {{1, 1}};
	mppa_noc_ret_t nret;
	odp_packet_hdr_t * pkt_hdr;
	unsigned int i;
	mppa_noc_uc_pointer_configuration_t uc_pointers = {{0}};
	unsigned int tx_index = pcie->port_id % NOC_UC_COUNT;
	unsigned int size;

	for (i = 0; i < pkt_count; i++) {
		pkt_hdr = odp_packet_hdr(pkt_table[i]);
		size = pkt_hdr->frame_len / sizeof(uint64_t);
		/* Setup parameters and pointers */
		if ((pkt_hdr->frame_len % sizeof(uint64_t)) != 0)
			size++;
			
		uc_conf.parameters[i] = size;
		uc_pointers.thread_pointers[i] =
			(uintptr_t) packet_map(pkt_hdr, 0, NULL) -
			(uintptr_t) &_data_start;
	}
	for (i = pkt_count; i < MAX_PKT_PER_UC; i++) {
		uc_conf.parameters[i] = 0;
		uc_pointers.thread_pointers[i] = 0;
	}

	odp_spinlock_lock(&pcie->wlock);
	INVALIDATE(g_uc_ctx);
	{
		pcie_uc_ctx_t * ctx = &g_uc_ctx[tx_index];
		unsigned job_id = ctx->job_id++;
		pcie_uc_job_ctx_t *job = &ctx->job_ctxs[job_id % MAX_JOB_PER_UC];

		/* Wait for previous run(s) to complete */
		while(ctx->joined_jobs + MAX_JOB_PER_UC <= job_id) {
			int ev_counter = mppa_noc_wait_clear_event(DNOC_CLUS_IFACE_ID,
								   MPPA_NOC_INTERRUPT_LINE_DNOC_TX,
								   ctx->dnoc_tx_id);
			for(i = ctx->joined_jobs; i < ctx->joined_jobs + ev_counter; ++i) {
				pcie_uc_job_ctx_t * joined_job = &ctx->job_ctxs[i % MAX_JOB_PER_UC];
				joined_job->is_running = 0;
				/* Free previous packets */
				ret_buf(&((pool_entry_t *)pcie->pool)->s,
					(odp_buffer_hdr_t**)joined_job->pkt_table, joined_job->pkt_count);
			}
			ctx->joined_jobs += ev_counter;
		}
		memcpy(job->pkt_table, pkt_table, pkt_count * sizeof(*pkt_table));

		uc_conf.pointers = &uc_pointers;
		uc_conf.event_counter = 0;

		nret = mppa_noc_dnoc_uc_configure(DNOC_CLUS_IFACE_ID, ctx->dnoc_uc_id,
						  uc_conf, pcie->header, pcie->config);
		if (nret != MPPA_NOC_RET_SUCCESS)
			return 1;

		job->pkt_count = pkt_count;
		job->is_running = 1;

		mppa_noc_dnoc_uc_set_program_run(DNOC_CLUS_IFACE_ID, ctx->dnoc_uc_id,
						 program_run);
	}
	odp_spinlock_unlock(&pcie->wlock);

	return 0;
}

static int pcie_send(pktio_entry_t *pktio_entry, odp_packet_t pkt_table[],
		    unsigned len)
{
	unsigned int sent = 0;
	pcie_status_t * pcie = pktio_entry->s.pkt_data;
	unsigned int pkt_count;


	while(sent < len) {
		pkt_count = (len - sent) > MAX_PKT_PER_UC ? MAX_PKT_PER_UC :
			(len - sent);

		pcie_send_packets(pcie, &pkt_table[sent], pkt_count);
		sent += pkt_count;
	}


	return sent;
}

static int pcie_promisc_mode_set(pktio_entry_t *const pktio_entry ODP_UNUSED,
				odp_bool_t enable ODP_UNUSED){
	return -1;
}

static int pcie_promisc_mode(pktio_entry_t *const pktio_entry ODP_UNUSED){
	return -1;
}

static int pcie_mtu_get(pktio_entry_t *const pktio_entry ODP_UNUSED) {
	return -1;
}
const pktio_if_ops_t pcie_pktio_ops = {
	.init = pcie_init,
	.term = NULL,
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
