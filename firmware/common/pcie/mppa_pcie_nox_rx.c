#include <stdio.h>
#include <errno.h>
#include <mppa_noc.h>

#include "mppa_pcie_buf_alloc.h"
#include "mppa_pcie_noc.h"


#define MAX_RX 					(30 * 4)
#define MAX_RX_IF_PER_THREAD	4
#define RX_THREAD_COUNT			2
#define RX_PER_IF				256

typedef struct rx_cfg {
		mppa_pcie_noc_rx_buf_t *mapped_buf;
} rx_cfg_t;

typedef struct rx_iface {
	/* 256 rx per interface */
	uint64_t ev_mask[4];
	rx_cfg_t rx_cfgs[RX_PER_IF];
} rx_iface_t;

typedef struct rx_thread {
	rx_iface_t iface[MPPA_PCIE_USABLE_DNOC_IF];
	int iface_idx[MPPA_PCIE_USABLE_DNOC_IF];
	int th_id;
} rx_thread_t;

rx_thread_t rx_threads[RX_THREAD_COUNT];

static int reload_rx(__attribute__((unused))  rx_thread_t *th, int dma_if, int rx_id)
{
	mppa_pcie_noc_rx_buf_t *buf;
	uint32_t left;
	int ret = buffer_ring_get_multi(&g_free_buf_pool, &buf, 1, &left);
	if (ret != 1) {
		printf("No more free buffer available\n");
		return -1;
	}	

	mppa_dnoc[dma_if]->rx_queues[rx_id].event_lac.hword;

	typeof(mppa_dnoc[dma_if]->rx_queues[0]) * const rx_queue =
		&mppa_dnoc[dma_if]->rx_queues[rx_id];

	rx_queue->buffer_base.dword = (uintptr_t) buf->buf_addr;

	/* Rearm the DMA Rx and check for droppped packets */
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
	
	return 0;
}


void mppa_pcie_noc_poll_masks(rx_thread_t *th, int dma_if)
{
	int i;
	uint64_t mask;
	int if_mask = 0;

	for (i = 0; i < 4; i++) {
		mask = mppa_dnoc[dma_if]->rx_global.events[i].dword & th->iface[dma_if].ev_mask[i];

		if (mask == 0ULL)
			continue;

		/* We have an event */
		while (mask != 0ULL) {
			const int mask_bit = __k1_ctzdl(mask);
			const int rx_id = mask_bit + i * 64;

			mask = mask ^ (1ULL << mask_bit);
			if_mask |=  reload_rx(th, dma_if, rx_id);
		}
	}

	//~ int if_mask_incomplete = 0;
	//~ while (if_mask) {
		//~ i = __builtin_k1_ctz(if_mask);
		//~ if_mask ^= (1 << i);

		//~ rx_buffer_list_t * hdr_list =
			//~ &rx_hdl.th[th_id].ifce[i].hdr_list;

		//~ if (hdr_list->tail == &hdr_list->head)
			//~ continue;

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

	return;
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
	int iface = if_id / RX_THREAD_COUNT;
	int rx_mask_off = *rx_id / 64;

	ret = mppa_noc_dnoc_rx_alloc_auto(if_id, rx_id, MPPA_NOC_NON_BLOCKING);
	if(ret) {
		fprintf(stderr, "[PCIe] Error: Failed to find an available Rx on if %d\n", if_id);
		return 1;
	}
	
	if (mppa_pcie_noc_configure_rx(&rx_threads[rx_thread_num].iface[iface], if_id, *rx_id)) {
		printf("failed to cofnigure noc rx\n");
		return 1;
	}

	// LOCK
	rx_threads[rx_thread_num].iface[iface].ev_mask[rx_mask_off] |= (1 << *rx_id);
	// UNLOCK
	return 0;
}

