#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

#include <mppa/osconfig.h>
#include <HAL/hal/hal.h>

#include "mppa_pcie_eth.h"
#include "mppa_pcie_noc.h"
#include "mppa_pcie_debug.h"
#include "netdev.h"

#define MAX_DNOC_TX_PER_PCIE_ETH_IF	16	

#define RING_BUFFER_ENTRIES	16

#define MPPA_PCIE_GET_ETH_VALUE(__mode, __member, __if) __builtin_k1_lwu(&g_eth_if_cfg[__if].__mode->__member)
#define MPPA_PCIE_ETH_GET_RX_TAIL(__if) MPPA_PCIE_GET_ETH_VALUE(rx, tail, __if)
#define MPPA_PCIE_ETH_GET_TX_TAIL(__if) MPPA_PCIE_GET_ETH_VALUE(tx, tail, __if)
#define MPPA_PCIE_ETH_GET_RX_HEAD(__if) MPPA_PCIE_GET_ETH_VALUE(rx, head, __if)
#define MPPA_PCIE_ETH_GET_TX_HEAD(__if) MPPA_PCIE_GET_ETH_VALUE(tx, head, __if)

#define MPPA_PCIE_SET_ETH_VALUE(__mode, __member, __if, __val) __builtin_k1_swu(&g_eth_if_cfg[__if].__mode->__member, __val)
#define MPPA_PCIE_ETH_SET_RX_TAIL(__if, __val) MPPA_PCIE_SET_ETH_VALUE(rx, tail, __if, __val)
#define MPPA_PCIE_ETH_SET_TX_TAIL(__if, __val) MPPA_PCIE_SET_ETH_VALUE(tx, tail, __if, __val)
#define MPPA_PCIE_ETH_SET_RX_HEAD(__if, __val) MPPA_PCIE_SET_ETH_VALUE(rx, head, __if, __val)
#define MPPA_PCIE_ETH_SET_TX_HEAD(__if, __val) MPPA_PCIE_SET_ETH_VALUE(tx, head, __if, __val)

#define MPPA_PCIE_ETH_SET_ENTRY_VALUE(__entry, __memb, __val) __builtin_k1_swu(&__entry->__memb, __val)
#define MPPA_PCIE_ETH_SET_DENTRY_VALUE(__entry, __memb, __val) __builtin_k1_sdu(&__entry->__memb, __val)
#define MPPA_PCIE_ETH_SET_ENTRY_LEN(__entry, __val) MPPA_PCIE_ETH_SET_ENTRY_VALUE(__entry, len, __val)
#define MPPA_PCIE_ETH_SET_ENTRY_FLAGS(__entry, __val) MPPA_PCIE_ETH_SET_ENTRY_VALUE(__entry, flags, __val)
#define MPPA_PCIE_ETH_SET_ENTRY_ADDR(__entry, __val) MPPA_PCIE_ETH_SET_DENTRY_VALUE(__entry, pkt_addr, __val)
/* Double reading */
#define MPPA_PCIE_ETH_GET_DENTRY_VALUE(__entry, __memb) __builtin_k1_ldu(&__entry->__memb)

/**
 * PCIe ethernet interface config
 */
struct mppa_pcie_g_eth_if_cfg {
	struct mppa_pcie_eth_dnoc_tx_cfg *dnoc_tx_cfg[MAX_DNOC_TX_PER_PCIE_ETH_IF];	/* Interfaces for the current */
	unsigned int dnoc_tx_count;
	unsigned int current_dnoc_tx; 	/* Current index in dnoc_tx_cfg array */
	struct mppa_pcie_eth_ring_buff_desc *rx, *tx;
};

static struct mppa_pcie_g_eth_if_cfg g_eth_if_cfg[MPPA_PCIE_ETH_IF_MAX] = {{{0}, 0, 0, NULL, NULL}};

int mppa_pcie_eth_init(int if_count)
{
#if defined(MAGIC_SCALL)
	return 0;
#endif
	if (if_count > MPPA_PCIE_ETH_IF_MAX)
		return 1;

	eth_if_cfg_t if_cfgs[if_count];
	for (int i = 0; i < if_count; ++i){
		if_cfgs[i].if_id = 0;
		if_cfgs[i].mtu = MPPA_PCIE_ETH_DEFAULT_MTU;
		if_cfgs[i].n_c2h_entries = RING_BUFFER_ENTRIES;
		if_cfgs[i].n_h2c_entries = RING_BUFFER_ENTRIES;
		if_cfgs[i].flags = MPPA_PCIE_ETH_CONFIG_RING_AUTOLOOP;
		if_cfgs[i].if_id = i;
		memcpy(if_cfgs[i].mac_addr, "\x02\xde\xad\xbe\xef", 5);
		if_cfgs[i].mac_addr[MAC_ADDR_LEN - 1] = i;
	}

	netdev_init(if_count, if_cfgs);
	for (int i = 0; i < if_count; ++i){
		g_eth_if_cfg[i].rx = (void*)(unsigned long)eth_control.configs[i].c2h_ring_buf_desc_addr;
		g_eth_if_cfg[i].tx = (void*)(unsigned long)eth_control.configs[i].h2c_ring_buf_desc_addr;
	}
		
	netdev_start();

	return 0;
}

int mppa_pcie_eth_tx_full(unsigned int pcie_eth_if)
{
	unsigned int rx_tail = MPPA_PCIE_ETH_GET_RX_TAIL(pcie_eth_if), next_rx_tail;
	unsigned int rx_head = MPPA_PCIE_ETH_GET_RX_HEAD(pcie_eth_if);

	/* Check if there is room to send a packet to host */
	next_rx_tail = (rx_tail + 1) % RING_BUFFER_ENTRIES;
	if (next_rx_tail == rx_head)
		return 1;
		
	return 0;
}

int mppa_pcie_eth_enqueue_tx(unsigned int pcie_eth_if, void *addr, unsigned int size, uint64_t data)
{
	unsigned int rx_tail = MPPA_PCIE_ETH_GET_RX_TAIL(pcie_eth_if), next_rx_tail;
	unsigned int rx_head = MPPA_PCIE_ETH_GET_RX_HEAD(pcie_eth_if);
	struct mppa_pcie_eth_c2h_ring_buff_entry *entry, *entries;
	uint64_t daddr = (uintptr_t) addr;

	/* Check if there is room to send a packet to host */
	next_rx_tail = (rx_tail + 1) % RING_BUFFER_ENTRIES;
	if (next_rx_tail == rx_head)
		return -1;

	//dbg_printf("Enqueuing tx for interface %p addr 0x%x to host rx descriptor %d\n", addr, size, rx_tail);
	entries = (void *) (uintptr_t) g_eth_if_cfg[pcie_eth_if].rx->ring_buffer_entries_addr;
	entry = &entries[rx_tail];
	
	/* If there are padding data, signal the enqueuer */ 
	if (entry->data != 0) {
		mppa_pcie_noc_rx_buffer_consumed(entry->data);
		entry->data = 0;
	}

	MPPA_PCIE_ETH_SET_ENTRY_LEN(entry, size);
	MPPA_PCIE_ETH_SET_ENTRY_ADDR(entry, daddr);
	MPPA_PCIE_ETH_SET_DENTRY_VALUE(entry, data, data);

	MPPA_PCIE_ETH_SET_RX_TAIL(pcie_eth_if, next_rx_tail);
	mppa_pcie_send_it_to_host();

	return 0;
}

int mppa_pcie_eth_enqueue_rx(unsigned int pcie_eth_if, void *addr, unsigned int size, uint32_t flags)
{
	unsigned int tx_tail = MPPA_PCIE_ETH_GET_TX_TAIL(pcie_eth_if);
	unsigned int tx_head = MPPA_PCIE_ETH_GET_TX_HEAD(pcie_eth_if), next_tx_head;
	struct mppa_pcie_eth_h2c_ring_buff_entry *entry, *entries;
	uint64_t daddr = (uintptr_t) addr;
	uint32_t prev_head;

	/* Do not add an entry if the ring is full */
	if (tx_head == tx_tail)
		return -1;
	
	if (tx_head == 0)
		prev_head = RING_BUFFER_ENTRIES - 1;
	else
		prev_head = tx_head - 1;

	//dbg_printf("Enqueuing rx for interface %d addr %p, size %d to host tx descriptor %ld\n", pcie_eth_if, addr, size, prev_head);
	entries = (void *) (uintptr_t) g_eth_if_cfg[pcie_eth_if].tx->ring_buffer_entries_addr;
	entry = &entries[prev_head];

	//dbg_printf("Entries: %p, Entry addr: %p\n", entries, entry);
	MPPA_PCIE_ETH_SET_ENTRY_LEN(entry, size);
	MPPA_PCIE_ETH_SET_ENTRY_ADDR(entry, daddr);
	MPPA_PCIE_ETH_SET_ENTRY_FLAGS(entry, flags);

	next_tx_head = (tx_head + 1) % RING_BUFFER_ENTRIES;
	MPPA_PCIE_ETH_SET_TX_HEAD(pcie_eth_if, next_tx_head);
	mppa_pcie_send_it_to_host();

	return 0;
}

/**
 * Enqueue tx fifo addr while possible
 */
static int mppa_pcie_eth_fill_rx(unsigned int pcie_eth_if_id)
{
	int ret;
	unsigned int dnoc_tx_id = g_eth_if_cfg[pcie_eth_if_id].current_dnoc_tx;
	struct mppa_pcie_eth_dnoc_tx_cfg *tx_cfg;

	if (g_eth_if_cfg[pcie_eth_if_id].dnoc_tx_count == 0)
		return 0;

	/* Fill RX descriptors by adding pcie fifo tx addr in a round robbin way */
	do {
		tx_cfg = g_eth_if_cfg[pcie_eth_if_id].dnoc_tx_cfg[dnoc_tx_id];

		/* Try to enqueue a fifo descriptor */
		ret = mppa_pcie_eth_enqueue_rx(pcie_eth_if_id, (void *) tx_cfg->fifo_addr, tx_cfg->mtu, MPPA_PCIE_ETH_NEED_PKT_HDR);
		if (ret == 0) {
			dnoc_tx_id++;
			dnoc_tx_id %= g_eth_if_cfg[pcie_eth_if_id].dnoc_tx_count;
		}
	} while(ret == 0);

	g_eth_if_cfg[pcie_eth_if_id].current_dnoc_tx = dnoc_tx_id;

	return 0;
}

int mppa_pcie_eth_handler()
{
	unsigned int i;

	for (i = 0; i < eth_control.if_count; i++) {
		mppa_pcie_eth_fill_rx(i);
	}

	return 0;
}

int mppa_pcie_eth_add_forward(unsigned int pcie_eth_if_id, struct mppa_pcie_eth_dnoc_tx_cfg *dnoc_tx_cfg)
{
	unsigned int dnoc_tx_count = g_eth_if_cfg[pcie_eth_if_id].dnoc_tx_count;

	if (dnoc_tx_count == MAX_DNOC_TX_PER_PCIE_ETH_IF) {
		dbg_printf("Too many forward for this PCIe eth interface\n");
		return 1;
	}

	g_eth_if_cfg[pcie_eth_if_id].dnoc_tx_cfg[dnoc_tx_count] = dnoc_tx_cfg;
	g_eth_if_cfg[pcie_eth_if_id].dnoc_tx_count++;

	mppa_pcie_eth_fill_rx(pcie_eth_if_id);

	return 0;
}
