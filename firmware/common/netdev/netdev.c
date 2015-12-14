#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include <mppa/osconfig.h>
#include <HAL/hal/hal.h>

#include "netdev.h"

#define DDR_BUFFER_BASE_ADDR	0x80000000

static uintptr_t g_current_pkt_addr = DDR_BUFFER_BASE_ADDR;

__attribute__((section(".eth_control"))) struct mppa_pcie_eth_control eth_control = {
	.magic = 0xDEADBEEF,
};

int netdev_setup_rx(struct mppa_pcie_eth_if_config *cfg, uint32_t n_entries,
		    uint32_t flags)
{
	struct mppa_pcie_eth_ring_buff_desc *rx;
	struct mppa_pcie_eth_rx_ring_buff_entry *entries;
	uint32_t i;

	if (cfg->mtu == 0) {
		fprintf(stderr, "[netdev] MTU not configured\n");
		return -1;
	}
	rx = calloc (1, sizeof(*rx));
	if (!rx)
		return -1;

	entries = calloc(n_entries, sizeof(*entries));
	if(!entries) {
		free(rx);
		return -1;
	}

	for(i = 0; i < n_entries; i++) {
		entries[i].pkt_addr = g_current_pkt_addr;
		g_current_pkt_addr += cfg->mtu;
#ifdef VERBOSE
		printf("RX Packet entry at 0x%"PRIx64"\n", entries[i].pkt_addr);
#endif
	}

	rx->ring_buffer_entries_count = n_entries;
	rx->ring_buffer_entries_addr = (uintptr_t) entries;
	(void)flags;
	/* rx->flags = flags; */
	cfg->rx_ring_buf_desc_addr = (uint64_t)(unsigned long)rx;

	return 0;
}

int netdev_setup_tx(struct mppa_pcie_eth_if_config *cfg, uint32_t n_entries,
		    uint32_t flags)
{
	struct mppa_pcie_eth_ring_buff_desc *tx;
	struct mppa_pcie_eth_tx_ring_buff_entry *entries;
	uint32_t i;

	if (cfg->mtu == 0) {
		fprintf(stderr, "[netdev] MTU not configured\n");
		return -1;
	}
	tx = calloc (1, sizeof(*tx));
	if (!tx)
		return -1;

	entries = calloc(n_entries, sizeof(*entries));
	if(!entries) {
		free(tx);
		return -1;
	}

	for(i = 0; i < n_entries; i++) {
		entries[i].pkt_addr = g_current_pkt_addr;
		g_current_pkt_addr += cfg->mtu;
#ifdef VERBOSE
		printf("TX Packet entry at 0x%"PRIx64"\n", entries[i].pkt_addr);
#endif
	}

	tx->ring_buffer_entries_count = n_entries;
	tx->ring_buffer_entries_addr = (uintptr_t) entries;
	(void)flags;
	/* tx->flags = flags; */
	cfg->tx_ring_buf_desc_addr = (uint64_t)(unsigned long)tx;

	return 0;
}

int netdev_init_interface(const eth_if_cfg_t *cfg)
{
	struct mppa_pcie_eth_if_config *if_cfg;
	int ret;

	if (cfg->if_id >= MPPA_PCIE_ETH_MAX_INTERFACE_COUNT)
		return -1;

	if_cfg = &eth_control.configs[cfg->if_id];
	if_cfg->mtu = cfg->mtu;
	if_cfg->flags = cfg->flags;
	if_cfg->interrupt_status = 1;
	memcpy(if_cfg->mac_addr, cfg->mac_addr, MAC_ADDR_LEN);

	ret = netdev_setup_rx(if_cfg, cfg->n_rx_entries, cfg->rx_flags);
	if (ret)
		return ret;

	ret = netdev_setup_tx(if_cfg, cfg->n_tx_entries, cfg->tx_flags);
	if (ret)
		return ret;
	return 0;
}

int netdev_init(uint8_t n_if, const eth_if_cfg_t cfg[n_if]) {
	uint8_t i;
	int ret;

	if (n_if > MPPA_PCIE_ETH_MAX_INTERFACE_COUNT)
		return -1;

	for (i = 0; i < n_if; ++i) {
		ret = netdev_init_interface(&cfg[i]);
		if (ret)
			return ret;
	}
	eth_control.if_count = n_if;

	return 0;
}

int netdev_start()
{
	/* Ensure coherency */
	__k1_mb();
	/* Cross fingers for everything to be setup correctly ! */
	__builtin_k1_swu(&eth_control.magic, 0xCAFEBABE);
	/* Ensure coherency */
	__k1_mb();

#ifdef __mos__
	mOS_pcie_write_usr_it(0);
	mOS_pcie_write_usr_it(1);
#else
	mppa_pcie_send_it_to_host();
#endif
	return 0;
}

