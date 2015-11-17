/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */
#include "HAL/hal/hal.h"
#include <odp/errno.h>
#include <errno.h>
#include <mppa_noc.h>

#include "odp_packet_io_internal.h"
#include "odp_pool_internal.h"
#include "odp_tx_uc_internal.h"

#include "ucode_fw/ucode_eth.h"
#include "ucode_fw/ucode_eth_v2.h"

extern char _heap_end;
static int tx_init = 0;
tx_uc_ctx_t g_tx_uc_ctx[NOC_UC_COUNT] = {{0}};

uint64_t tx_uc_alloc_uc_slots(tx_uc_ctx_t *ctx,
			      unsigned int count)
{
	ODP_ASSERT(count <= MAX_JOB_PER_UC);

	const uint64_t head =
		odp_atomic_fetch_add_u64(&ctx->head, count);
	const uint32_t last_id = head + count - 1;
	unsigned  ev_counter, diff;

	/* Wait for slot */
	ev_counter = mOS_uc_read_event(ctx->dnoc_uc_id);
	diff = last_id - ev_counter;
	while (diff > 0x80000000 || ev_counter + MAX_JOB_PER_UC <= last_id) {
		odp_spin();
		ev_counter = mOS_uc_read_event(ctx->dnoc_uc_id);
		diff = last_id - ev_counter;
	}

	/* Free previous packets */
	for (uint64_t pos = head; pos < head + count; pos++) {
		if(pos > MAX_JOB_PER_UC){
			tx_uc_job_ctx_t *job = &ctx->job_ctxs[pos % MAX_JOB_PER_UC];
			INVALIDATE(job);
			if (!job->pkt_count || job->nofree)
				continue;

			packet_free_multi(job->pkt_table,
					  job->pkt_count);
		}
	}
	return head;
}
void tx_uc_commit(tx_uc_ctx_t *ctx, uint64_t slot,
		  unsigned int count)
{
#if MOS_UC_VERSION == 1
	while (odp_atomic_load_u64(&ctx->commit_head) != slot)
		odp_spin();
#endif

	__builtin_k1_wpurge();
	__builtin_k1_fence ();

#if MOS_UC_VERSION == 1
	for (unsigned i = 0; i < count; ++i)
		mOS_ucore_commit(ctx->dnoc_tx_id);
	odp_atomic_fetch_add_u64(&ctx->commit_head, count);
#else
	for (unsigned i = 0, pos = slot % MAX_JOB_PER_UC; i < count;
	     ++i, pos = (pos + 1) % MAX_JOB_PER_UC) {
		mOS_uc_transaction_t * const trs =
			&_scoreboard_start.SCB_UC.trs [ctx->dnoc_uc_id][pos];
		mOS_ucore_commit(ctx->dnoc_tx_id, trs);
	}
#endif
}

int tx_uc_init(void)
{
	int i;
	mppa_noc_ret_t ret;
	mppa_noc_dnoc_uc_configuration_t uc_conf =
		MPPA_NOC_DNOC_UC_CONFIGURATION_INIT;

	if (tx_init)
		return -1;

#if MOS_UC_VERSION == 1
	uc_conf.program_start = (uintptr_t)ucode_eth;
#else
	uc_conf.program_start = (uintptr_t)ucode_eth_v2;
#endif
	uc_conf.buffer_base = (uintptr_t)&_data_start;
	uc_conf.buffer_size = (uintptr_t)&_heap_end - (uintptr_t)&_data_start;

	for (i = 0; i < NOC_UC_COUNT; i++) {

		odp_atomic_init_u64(&g_tx_uc_ctx[i].head, 0);
#if MOS_UC_VERSION == 1
		odp_atomic_init_u64(&g_tx_uc_ctx[i].commit_head, 0);
#endif
		/* DNoC */
		ret = mppa_noc_dnoc_tx_alloc_auto(DNOC_CLUS_IFACE_ID,
						  &g_tx_uc_ctx[i].dnoc_tx_id,
						  MPPA_NOC_BLOCKING);
		if (ret != MPPA_NOC_RET_SUCCESS)
			return 1;

		ret = mppa_noc_dnoc_uc_alloc_auto(DNOC_CLUS_IFACE_ID,
						  &g_tx_uc_ctx[i].dnoc_uc_id,
						  MPPA_NOC_BLOCKING);
		if (ret != MPPA_NOC_RET_SUCCESS)
			return 1;

		/* We will only use events */
		mppa_noc_disable_interrupt_handler(DNOC_CLUS_IFACE_ID,
						   MPPA_NOC_INTERRUPT_LINE_DNOC_TX,
						   g_tx_uc_ctx[i].dnoc_uc_id);


		ret = mppa_noc_dnoc_uc_link(DNOC_CLUS_IFACE_ID,
					    g_tx_uc_ctx[i].dnoc_uc_id,
					    g_tx_uc_ctx[i].dnoc_tx_id, uc_conf);
		if (ret != MPPA_NOC_RET_SUCCESS)
			return 1;

#if MOS_UC_VERSION == 2
		for (int j = 0; j < MOS_NB_UC_TRS; j++) {
			mOS_uc_transaction_t  * trs =
				& _scoreboard_start.SCB_UC.trs[g_tx_uc_ctx[i].dnoc_uc_id][j];
			trs->notify._word = 0;
			trs->desc.tx_set = 1 << g_tx_uc_ctx[i].dnoc_tx_id;
			trs->desc.param_count = 8;
			trs->desc.pointer_count = 4;
		}
#endif
	}

	tx_init = 1;
	return 0;
}

int tx_uc_send_packets(const pkt_tx_uc_config *tx_config,
		       tx_uc_ctx_t *ctx, odp_packet_t pkt_table[],
		       int pkt_count, int mtu, int *err)
{
	const uint64_t head = tx_uc_alloc_uc_slots(ctx, 1);
	const unsigned slot_id = head % MAX_JOB_PER_UC;
	tx_uc_job_ctx_t * job = &ctx->job_ctxs[slot_id];
	mOS_uc_transaction_t * const trs =
		&_scoreboard_start.SCB_UC.trs [ctx->dnoc_uc_id][slot_id];
	const odp_packet_hdr_t * pkt_hdr;

	*err = 0;
	job->nofree = tx_config->nofree;
	for (int i = 0; i < pkt_count; ++i ){
		job->pkt_table[i] = pkt_table[i];
		pkt_hdr = odp_packet_hdr(pkt_table[i]);

		if (pkt_hdr->frame_len > mtu) {
			pkt_count = i;
			*err = EINVAL;
			break;
		}

		trs->parameter.array[2 * i + 0] =
			pkt_hdr->frame_len / sizeof(uint64_t);
		trs->parameter.array[2 * i + 1] =
			pkt_hdr->frame_len % sizeof(uint64_t);

		trs->pointer.array[i] = (unsigned long)
			(((uint8_t*)pkt_hdr->buf_hdr.addr + pkt_hdr->headroom)
			 - (uint8_t*)&_data_start);
	}
	for (int i = pkt_count; i < 4; ++i) {
		trs->parameter.array[2 * i + 0] = 0;
		trs->parameter.array[2 * i + 1] = 0;
	}

	trs->path.array[ctx->dnoc_tx_id].header = tx_config->header;
	trs->path.array[ctx->dnoc_tx_id].config = tx_config->config;
#if MOS_UC_VERSION == 1
	trs->notify._word = 0;
	trs->desc.tx_set = 1 << ctx->dnoc_tx_id;
	trs->desc.param_set = 0xff;
	trs->desc.pointer_set = (0x1 <<  pkt_count) - 1;
#endif

	job->pkt_count = pkt_count;

	tx_uc_commit(ctx, head, 1);
	return pkt_count;
}

void tx_uc_flush(tx_uc_ctx_t *ctx)
{
	const uint64_t head = tx_uc_alloc_uc_slots(ctx, MAX_JOB_PER_UC);

	for (int slot_id = 0; slot_id < MAX_JOB_PER_UC; ++slot_id) {
		tx_uc_job_ctx_t * job = &ctx->job_ctxs[slot_id];
		mOS_uc_transaction_t * const trs =
			&_scoreboard_start.SCB_UC.trs[ctx->dnoc_uc_id][slot_id];
		for (unsigned i = 0; i < MAX_PKT_PER_UC; ++i ){
			trs->parameter.array[2 * i + 0] = 0;
			trs->parameter.array[2 * i + 1] = 0;
		}
		trs->notify._word = 0;
		trs->desc.tx_set = 0;
#if MOS_UC_VERSION == 1
		trs->desc.param_set = 0xff;
		trs->desc.pointer_set = 0;
#endif
		job->pkt_count = 0;
		job->nofree = 1;
	}
	tx_uc_commit(ctx, head, MAX_JOB_PER_UC);
}
