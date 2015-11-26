#include <stdio.h>
#include <errno.h>
#include <mppa_noc.h>

#include "mppa_pcie_buf_alloc.h"
#include "mppa_pcie_noc.h"

#define MAX_RX 					(30 * 4)
#define MAX_RX_IF_PER_THREAD	2
#define RX_THREAD_COUNT			4

typedef struct rx_iface {
	/* 256 rx per interface */
	uint64_t ev_mask[4];
} rx_iface_t;

typedef struct rx_thread {
	rx_iface_t ifce[MAX_RX_IF_PER_THREAD];
} rx_thread_t;

rx_thread_t rx_threads[RX_THREAD_COUNT];

int mppa_pcie_noc_configure_rx(int dma_if, int rx_id)
{
	mppa_pcie_noc_rx_buf_t *buf;
	uint32_t left;
	mppa_noc_dnoc_rx_configuration_t conf = MPPA_NOC_DNOC_RX_CONFIGURATION_INIT;
	int ret = buffer_ring_get_multi(&g_free_buf_pool, &buf, 1, &left);
	if (ret != 1) {
		printf("No more free buffer available\n");
		return -1;
	}
	
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

		/* WARNING */
	ret = mppa_dnoc[dma_if]->rx_queues[rx_id].
		get_drop_pkt_nb_and_activate.reg;
	mppa_noc_enable_event(dma_if,
			      MPPA_NOC_INTERRUPT_LINE_DNOC_RX,
			      rx_id, (1 << BSP_NB_PE_P) - 1);

	return 0;
}


int mppa_pcie_noc_(int dma_if, int n_ports)
{
	if (n_ports > MAX_RX) {
		printf("asking for too many Rx port");
		return -1;
	}

	/*
	 * Allocate contiguous RX ports
	 */
	int n_rx, first_rx;

	for (first_rx = 0; first_rx <  MPPA_DNOC_RX_QUEUES_NUMBER - n_ports;
	     ++first_rx) {
		for (n_rx = 0; n_rx < n_ports; ++n_rx) {
			mppa_noc_ret_t ret;
			ret = mppa_noc_dnoc_rx_alloc(dma_if,
						     first_rx + n_rx);
			if (ret != MPPA_NOC_RET_SUCCESS)
				break;
		}
		if (n_rx < n_ports) {
			n_rx--;
			for ( ; n_rx >= 0; --n_rx) {
				mppa_noc_dnoc_rx_free(dma_if,
						      first_rx + n_rx);
			}
		} else {
			break;
		}
	}
	if (n_rx < n_ports) {
		printf("failed to allocate %d contiguous Rx ports\n", n_ports);
		return -1;
	}
	
	return 0;
}


int mppa_pcie_eth_setup_rx(int if_id, unsigned int *rx_id)
{
	mppa_noc_ret_t ret;

	ret = mppa_noc_dnoc_rx_alloc_auto(if_id, rx_id, MPPA_NOC_NON_BLOCKING);
	if(ret) {
		fprintf(stderr, "[PCIe] Error: Failed to find an available Rx on if %d\n", if_id);
		return 1;
	}
	if (mppa_pcie_noc_configure_rx(if_id, *rx_id)) {
		printf("failed to cofnigure noc rx\n");
		return 1;
	}

	return 0;
}

