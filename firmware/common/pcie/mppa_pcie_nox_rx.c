#include <stdio.h>
#include <errno.h>
#include <mppa_noc.h>

#include "mppa_pcie_buf_alloc.h"
#include "mppa_pcie_noc.h"


#define MAX_RX 					(30 * 4)
#define RX_THREAD_COUNT			2
#define IF_PER_THREAD			(MPPA_PCIE_USABLE_DNOC_IF / RX_THREAD_COUNT)
#define RX_PER_IF				256

typedef struct rx_cfg {
		mppa_pcie_noc_rx_buf_t *mapped_buf;
} rx_cfg_t;

typedef struct rx_iface {
	/* 256 rx per interface */
	uint64_t ev_mask[4];
	rx_cfg_t rx_cfgs[RX_PER_IF];
	int iface_id;
} rx_iface_t;

typedef struct rx_thread {
	rx_iface_t iface[IF_PER_THREAD];
	int th_id;
	volatile int ready;
} rx_thread_t;

rx_thread_t g_rx_threads[RX_THREAD_COUNT];

static int reload_rx(rx_iface_t *iface, int rx_id)
{
	mppa_pcie_noc_rx_buf_t *buf;
	uint32_t left;
	int ret = buffer_ring_get_multi(&g_free_buf_pool, &buf, 1, &left);
	if (ret != 1) {
		printf("No more free buffer available\n");
		return -1;
	}

	mppa_dnoc[iface->iface_id]->rx_queues[rx_id].event_lac.hword;

	typeof(mppa_dnoc[iface->iface_id]->rx_queues[0]) * const rx_queue =
		&mppa_dnoc[iface->iface_id]->rx_queues[rx_id];

	rx_queue->buffer_base.dword = (uintptr_t) buf->buf_addr;

	/* Rearm the DMA Rx and check for dropped packets */
	rx_queue->current_offset.reg = 0ULL;
	rx_queue->buffer_size.dword = MPPA_PCIE_MULTIBUF_SIZE;

	int dropped = rx_queue->
		get_drop_pkt_nb_and_activate.reg;

	if (dropped) {
		/* Really force those values.
		 * Item counter must be 2 in this case. */
		int j;
		/* WARNING */
		for (j = 0; j < 16; ++j)
			rx_queue->item_counter.reg = 2;
		for (j = 0; j < 16; ++j)
			rx_queue->activation.reg = 0x1;
	}

	iface->rx_cfgs[rx_id].mapped_buf = buf;

	return 0;
}


void mppa_pcie_noc_poll_masks(rx_thread_t *th, rx_iface_t *iface)
{
	int i;
	uint64_t mask;
	int if_mask = 0;
	iface  = iface;
	int dma_if = iface->iface_id;
	
	
	printf("Polling if %d\n", iface->iface_id);

	for (i = 0; i < 4; i++) {
		mask = mppa_dnoc[dma_if]->rx_global.events[i].dword & th->iface[dma_if].ev_mask[i];

		if (mask == 0ULL)
			continue;

		/* We have an event */
		while (mask != 0ULL) {
			const int mask_bit = __k1_ctzdl(mask);
			const int rx_id = mask_bit + i * 64;

			mask = mask ^ (1ULL << mask_bit);
			if_mask |=  reload_rx(iface, rx_id);
		}
	}

	//~ int if_mask_incomplete = 0;
	//~ while (if_mask) {
		//~ i = __builtin_k1_ctz(if_mask);
		//~ if_mask ^= (1 << i);

		//~ hdr_list->head = odp_buffer_ring_push_list(&rx_hdl.ifce[i].ring,
							   //~ hdr_list->head,
							   //~ &hdr_list->count);
		//~ if (!hdr_list->count) {
			//~ /* All were flushed */
			//~ hdr_list->tail = &hdr_list->head;
			//~ hdr_list->count = 0;
			//~ if_mask ^= (1 << i);
		//~ } else {
			//~ /* Not all buffers were flushed to the ring */
			//~ if_mask_incomplete = 1 << i;
		//~ }
	//~ }
	//~ if_mask = if_mask_incomplete;
}

void
mppa_pcie_rx_rm_func()
{
	rx_thread_t *thread = &g_rx_threads[__k1_get_cpu_id() - RX_RM_START];
	int iface;

	printf("RM %d with thread id %d started\n", __k1_get_cpu_id(), thread->th_id);

	thread->ready = 1;
	while (1) {
		for (iface = 0; iface < IF_PER_THREAD; iface++) {
			mppa_pcie_noc_poll_masks(thread, &thread->iface[iface]);
		}
	}
}

static uint64_t g_stacks[RX_RM_COUNT][RX_RM_STACK_SIZE];

void
mppa_pcie_noc_start_rx_rm()
{
	unsigned int rm_num, if_start = 0;
	unsigned int i;

	rx_thread_t *thread;
	for (rm_num = RX_RM_START; rm_num < RX_RM_START + RX_RM_COUNT; rm_num++ ){
		thread = &g_rx_threads[rm_num - RX_RM_START];

		thread->th_id = rm_num - RX_RM_START;

		for( i = 0; i < IF_PER_THREAD; i++) {
			thread->iface[i].iface_id = if_start++;
		}

		/* Init with scratchpad size */
		_K1_PE_STACK_ADDRESS[rm_num] = &g_stacks[thread->th_id][RX_RM_STACK_SIZE - 16];
		_K1_PE_START_ADDRESS[rm_num] = &mppa_pcie_rx_rm_func;
		_K1_PE_ARGS_ADDRESS[rm_num] = 0;

		__builtin_k1_dinval();
		__builtin_k1_wpurge();
		__builtin_k1_fence();

		printf("Powering RM %d\n", rm_num);
		__k1_poweron(rm_num);
	}

	for (rm_num = RX_RM_START; rm_num < RX_RM_START + RX_RM_COUNT; rm_num++ ){
		while(!g_rx_threads[rm_num - RX_RM_START].ready);
		printf("RM %d started\n", rm_num);
	}
}


int mppa_pcie_noc_configure_rx(rx_iface_t *iface, int dma_if, int rx_id)
{
	mppa_pcie_noc_rx_buf_t *buf;
	uint32_t left;
	mppa_noc_dnoc_rx_configuration_t conf = MPPA_NOC_DNOC_RX_CONFIGURATION_INIT;
	int ret = buffer_ring_get_multi(&g_free_buf_pool, &buf, 1, &left);
	if (ret != 1) {
		printf("No more free buffer available\n");
		return -1;
	}
	
	iface->rx_cfgs[rx_id].mapped_buf = buf;

	conf.buffer_base = (uintptr_t) buf->buf_addr;
	conf.buffer_size = MPPA_PCIE_MULTIBUF_SIZE;
	conf.current_offset = 0;
	conf.event_counter = 0;
	conf.item_counter = 1;
	conf.item_reload = 1;
	conf.reload_mode = MPPA_NOC_RX_RELOAD_MODE_INCR_DATA_NOTIF;
	conf.activation = 0x3;
	conf.counter_id = 0;

	ret = mppa_noc_dnoc_rx_configure(dma_if, rx_id, conf);
	if (ret)
		return -1;

	ret = mppa_dnoc[dma_if]->rx_queues[rx_id].
		get_drop_pkt_nb_and_activate.reg;
	mppa_noc_enable_event(dma_if,
			      MPPA_NOC_INTERRUPT_LINE_DNOC_RX,
			      rx_id, (1 << BSP_NB_PE_P) - 1);

	return 0;
}


int mppa_pcie_eth_setup_rx(int if_id, unsigned int *rx_id)
{
	mppa_noc_ret_t ret;
	int rx_thread_num = if_id % RX_THREAD_COUNT;
	int th_iface_id = if_id / RX_THREAD_COUNT;
	int rx_mask_off;
	rx_iface_t *iface;

	ret = mppa_noc_dnoc_rx_alloc_auto(if_id, rx_id, MPPA_NOC_NON_BLOCKING);
	if(ret) {
		fprintf(stderr, "[PCIe] Error: Failed to find an available Rx on if %d\n", if_id);
		return 1;
	}
	printf("RX %d allocated on iface %d, will use thread %d\n", *rx_id, if_id, rx_thread_num);

	rx_mask_off = *rx_id / 64;
	iface = &g_rx_threads[rx_thread_num].iface[th_iface_id];

	if (mppa_pcie_noc_configure_rx(iface, if_id, *rx_id)) {
		printf("failed to configure noc rx\n");
		return 1;
	}

	iface->ev_mask[rx_mask_off] |= (1 << *rx_id);

	return 0;
}
