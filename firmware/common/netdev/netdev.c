#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

#include <mppa/osconfig.h>
#include <HAL/hal/hal.h>

#include "netdev.h"

#define DDR_BUFFER_BASE_ADDR	0x80000000

static uintptr_t g_current_pkt_addr = DDR_BUFFER_BASE_ADDR;

struct mppa_pcie_eth_ring_buff_desc *g_netdev_rx[IF_COUNT];
struct mppa_pcie_eth_ring_buff_desc *g_netdev_tx[IF_COUNT];

__attribute__((section(".eth_control"))) struct mppa_pcie_eth_control eth_control = {
	.magic = 0xDEADBEEF,
};

static void setup_rx(struct mppa_pcie_eth_ring_buff_desc *rx)
{
	struct mppa_pcie_eth_rx_ring_buff_entry *entries;
	int i;

	entries = calloc(RING_BUFFER_ENTRIES, sizeof(struct mppa_pcie_eth_rx_ring_buff_entry));
	if(!entries)
		assert(0);

	for(i = 0; i < RING_BUFFER_ENTRIES; i++) {
		entries[i].pkt_addr = g_current_pkt_addr;
		printf("RX Packet entry at 0x%"PRIx64"\n", entries[i].pkt_addr);
		g_current_pkt_addr += MPPA_PCIE_ETH_DEFAULT_MTU;
	}

	rx->ring_buffer_entries_count = RING_BUFFER_ENTRIES;
	rx->ring_buffer_entries_addr = (uintptr_t) entries;
}

static void setup_tx(struct mppa_pcie_eth_ring_buff_desc *tx)
{
	struct mppa_pcie_eth_tx_ring_buff_entry *entries;
	int i;

	entries = calloc(RING_BUFFER_ENTRIES, sizeof(struct mppa_pcie_eth_tx_ring_buff_entry));
	if(!entries)
		assert(0);

	for(i = 0; i < RING_BUFFER_ENTRIES; i++) {
		entries[i].pkt_addr = g_current_pkt_addr;
		printf("TX Packet entry at 0x%"PRIx64"\n", entries[i].pkt_addr);
		g_current_pkt_addr += MPPA_PCIE_ETH_DEFAULT_MTU;
	}

	tx->ring_buffer_entries_count = RING_BUFFER_ENTRIES;
	tx->ring_buffer_entries_addr = (uintptr_t) entries;
}

int netdev_init_configs()
{
	int i;
	struct mppa_pcie_eth_ring_buff_desc *desc_ptr;

	for(i = 0; i < IF_COUNT; i++) {
		eth_control.configs[i].mtu = MPPA_PCIE_ETH_DEFAULT_MTU;

		desc_ptr = calloc(2, sizeof(struct mppa_pcie_eth_ring_buff_desc));
		if(!desc_ptr)
			assert(0);

		printf("Rx desc at %p and TX desc at %p\n", &desc_ptr[0], &desc_ptr[1]);
		setup_rx(&desc_ptr[0]);
		setup_tx(&desc_ptr[1]);

		g_netdev_rx[i] = &desc_ptr[0];
		g_netdev_tx[i] = &desc_ptr[1];

		eth_control.configs[i].rx_ring_buf_desc_addr = (uintptr_t) &desc_ptr[0];
		eth_control.configs[i].tx_ring_buf_desc_addr = (uintptr_t) &desc_ptr[1];
	}
	/* Ensure coherency */
	__k1_mb();
	/* Cross fingers for everything to be setup correctly ! */
	__builtin_k1_swu(&eth_control.magic, 0xCAFEBABE);
	return 0;
}
