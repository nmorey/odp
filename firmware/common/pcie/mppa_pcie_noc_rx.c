#include <stdio.h>
#include <errno.h>
#include <mppa_noc.h>

#include "mppa_pcie_buf_alloc.h"
#include "mppa_pcie_noc.h"
#include "mppa_pcie_debug.h"


#define MAX_RX 					(30 * 4)
#define RX_THREAD_COUNT			2
#define IF_PER_THREAD			(MPPA_PCIE_USABLE_DNOC_IF / RX_THREAD_COUNT)
#define RX_PER_IF				256

#define MPPA_PCIE_RM_COUNT		4

#define RX_RM_START		1
#define RX_RM_COUNT		2
#define PCIE_TX_RM		(RX_RM_START + RX_RM_COUNT)

/**
 * WARNING: struct from odp_tx_uc_internal
 */
 
#define END_OF_PACKETS		(1 << 0)

typedef union {
	struct {
		uint16_t pkt_size;
		uint16_t flags;
	};
	uint64_t dword;
} tx_uc_header_t;

typedef struct rx_cfg {
	mppa_pcie_noc_rx_buf_t *mapped_buf;
	uint8_t pcie_eth_if; /* PCIe ethernet interface */
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
	uint16_t events;
	uint16_t pcie_eth_if = iface->rx_cfgs[rx_id].pcie_eth_if;
	
	int ret = buffer_ring_get_multi(&g_free_buf_pool, &buf, 1, &left);
	if (ret != 1) {
		dbg_printf("No more free buffer available\n");
		return -1;
	}
	
	events = mppa_noc_dnoc_rx_lac_event_counter(iface->iface_id, rx_id);
	if (!events) {
		dbg_printf("Invalid count of events\n");
		exit(1);
	}

	dbg_printf("Adding buf to eth if %d\n", pcie_eth_if);
	/* Add previous buffer to full list */
	buffer_ring_push_multi(&g_full_buf_pool[pcie_eth_if], &iface->rx_cfgs[rx_id].mapped_buf, 1, &left);
	
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
	dbg_printf("Reloading rx %d of if %d with buffer %p\n", rx_id, iface->iface_id, buf);

	return 0;
}


static void mppa_pcie_noc_poll_masks(rx_iface_t *iface)
{
	int i;
	int dma_if = iface->iface_id;
	mppa_noc_dnoc_rx_bitmask_t bitmask;

	bitmask = mppa_noc_dnoc_rx_get_events_bitmask(dma_if);

	for (i = 0; i < 3; ++i) {
		bitmask.bitmask[i] &= iface->ev_mask[i];
		while(bitmask.bitmask[i]) {
			const int mask_bit = __k1_ctzdl(bitmask.bitmask[i]);
			int rx_id = mask_bit + i * 8 * sizeof(bitmask.bitmask[i]);
			bitmask.bitmask[i] &= ~(1 << mask_bit);

			reload_rx(iface, rx_id);
		}
	}
}

void mppa_pcie_noc_rx_buffer_consumed(uint64_t data)
{
	mppa_pcie_noc_rx_buf_t *buf;
	uint32_t left;

	buf = (mppa_pcie_noc_rx_buf_t *) (uintptr_t) data ;

	buf->pkt_count--;
	/* All packets from this buffer have been transfered, add it again to the free list */
	if (buf->pkt_count == 0)
		buffer_ring_push_multi(&g_free_buf_pool, &buf, 1, &left);
}


static void poll_noc_rx_buffer(int pcie_eth_if)
{	
	mppa_pcie_noc_rx_buf_t *bufs[MPPA_PCIE_MULTIBUF_COUNT], *buf;
	uint32_t left, pkt_size;
	int ret, buf_idx;
	void * pkt_addr;
	tx_uc_header_t hdr;

	if (mppa_pcie_eth_tx_full(pcie_eth_if)) {
		dbg_printf("PCIe eth tx is full !!!\n");
		return;
	}

	ret = buffer_ring_get_multi(&g_full_buf_pool[pcie_eth_if], bufs, MPPA_PCIE_MULTIBUF_COUNT, &left);
	if (ret == 0)
		return;

	dbg_printf("%d buffer ready to be sent\n", ret);
	for(buf_idx = 0; buf_idx < ret; buf_idx++) {
		buf = bufs[buf_idx];
		buf->pkt_count = 0;

		/* Packet size is added as a header to the packet */
		pkt_addr = buf->buf_addr;

		while (1) {
			/* Read ehader from packet */
			hdr.dword = __builtin_k1_ldu(pkt_addr);
			pkt_size = hdr.pkt_size;
			pkt_addr += sizeof(tx_uc_header_t);
			buf->pkt_count++;

			dbg_printf("packet at addr %p, size %ld\n", pkt_addr, pkt_size);
			/* Send one packet of the buffer and add buf as padding data to handle consumed packets */
			mppa_pcie_eth_enqueue_tx(pcie_eth_if, pkt_addr, pkt_size, (uintptr_t) buf);
			pkt_addr += sizeof(pkt_size);
			if (hdr.flags & END_OF_PACKETS)
				break;
		}

		dbg_printf("%d packets handled\n", buf->pkt_count);
	}
}

/**
 * Sender RM function which take buffer from clusters and send them
 * to host through PCIe.
 */
static void mppa_pcie_pcie_tx_sender()
{
	unsigned int i = 0;

	while(1) {
		for (i = 0; i < g_pcie_if_count; i++)
			poll_noc_rx_buffer(i);
	}
}

static uint64_t g_pcie_tx_stack[RX_RM_STACK_SIZE];

void mppa_pcie_start_pcie_tx_rm()
{
		/* Init with scratchpad size */
		_K1_PE_STACK_ADDRESS[PCIE_TX_RM] = &g_pcie_tx_stack[RX_RM_STACK_SIZE - 16];
		_K1_PE_START_ADDRESS[PCIE_TX_RM] = &mppa_pcie_pcie_tx_sender;
		_K1_PE_ARGS_ADDRESS[PCIE_TX_RM] = 0;

		__builtin_k1_dinval();
		__builtin_k1_wpurge();
		__builtin_k1_fence();

		dbg_printf("Powering pcie tx RM %d\n", PCIE_TX_RM);
		__k1_poweron(PCIE_TX_RM);
}

void
mppa_pcie_rx_rm_func()
{
	rx_thread_t *thread = &g_rx_threads[__k1_get_cpu_id() - RX_RM_START];
	int iface;

	dbg_printf("RM %d with thread id %d started\n", __k1_get_cpu_id(), thread->th_id);

	thread->ready = 1;
	while (1) {
		for (iface = 0; iface < IF_PER_THREAD; iface++) {
			mppa_pcie_noc_poll_masks(&thread->iface[iface]);
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

		dbg_printf("Powering RM %d\n", rm_num);
		__k1_poweron(rm_num);
	}

	for (rm_num = RX_RM_START; rm_num < RX_RM_START + RX_RM_COUNT; rm_num++ ){
		while(!g_rx_threads[rm_num - RX_RM_START].ready);
		dbg_printf("RM %d started\n", rm_num);
	}
}

int mppa_pcie_noc_configure_rx(rx_iface_t *iface, int dma_if, int rx_id)
{
	mppa_pcie_noc_rx_buf_t *buf;
	uint32_t left;
	mppa_noc_dnoc_rx_configuration_t conf = MPPA_NOC_DNOC_RX_CONFIGURATION_INIT;
	int ret = buffer_ring_get_multi(&g_free_buf_pool, &buf, 1, &left);
	if (ret != 1) {
		dbg_printf("No more free buffer available\n");
		return -1;
	}

	iface->rx_cfgs[rx_id].mapped_buf = buf;
	iface->rx_cfgs[rx_id].pcie_eth_if = iface->rx_cfgs[rx_id].pcie_eth_if;

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


int mppa_pcie_eth_setup_rx(int if_id, unsigned int *rx_id, unsigned int pcie_eth_if)
{
	mppa_noc_ret_t ret;
	int rx_thread_num = if_id / RX_THREAD_COUNT;
	int th_iface_id = if_id % IF_PER_THREAD;
	int rx_mask_off;
	rx_iface_t *iface;

	ret = mppa_noc_dnoc_rx_alloc_auto(if_id, rx_id, MPPA_NOC_NON_BLOCKING);
	if(ret) {
		fprintf(stderr, "[PCIe] Error: Failed to find an available Rx on if %d\n", if_id);
		return 1;
	}
	dbg_printf("RX %d allocated on iface %d, using thread %d\n", *rx_id, if_id, rx_thread_num);

	rx_mask_off = *rx_id / (sizeof(iface->ev_mask[0]) * 8);
	iface = &g_rx_threads[rx_thread_num].iface[th_iface_id];

	if (mppa_pcie_noc_configure_rx(iface, if_id, *rx_id)) {
		dbg_printf("failed to configure noc rx\n");
		return 1;
	}

	iface->ev_mask[rx_mask_off] |= (1 << *rx_id);
	iface->rx_cfgs[*rx_id].pcie_eth_if = pcie_eth_if;
	__k1_mb();

	return 0;
}
