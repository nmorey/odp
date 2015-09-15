#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

#include <mppa/osconfig.h>
#include <HAL/hal/hal.h>

#include "mppa_pcie_eth.h"

#define IF_COUNT_MAX 16

#define RING_BUFFER_ENTRIES	32
#define DDR_BUFFER_BASE_ADDR	0x80000000


__attribute__((section(".eth_control"))) struct mppa_pcie_eth_control eth_control = {
	.magic = 0xDEADBEEF,
};

static struct mppa_pcie_eth_ring_buff_desc *g_rx[IF_COUNT_MAX], *g_tx[IF_COUNT_MAX];

int g_interrupt_flags[IF_COUNT_MAX] = {0};

static unsigned int g_if_count; 

static void setup_rx(struct mppa_pcie_eth_ring_buff_desc *rx)
{
	struct mppa_pcie_eth_rx_ring_buff_entry *entries;

	entries = calloc(RING_BUFFER_ENTRIES, sizeof(struct mppa_pcie_eth_rx_ring_buff_entry));
	if(!entries)
		assert(0);

	/* No rx packet for host by default */
	rx->head = 0;
	rx->tail = 0;
 
	rx->ring_buffer_entries_count = RING_BUFFER_ENTRIES;
	rx->ring_buffer_entries_addr = (uintptr_t) entries;
}

static void setup_tx(struct mppa_pcie_eth_ring_buff_desc *tx)
{
	struct mppa_pcie_eth_tx_ring_buff_entry *entries;

	entries = calloc(RING_BUFFER_ENTRIES, sizeof(struct mppa_pcie_eth_tx_ring_buff_entry));
	if(!entries)
		assert(0);

	/* Set the tx as full to avoid the host sending packets
	 * We will fill the dexcriptor later */
	tx->head = 0;
	tx->tail = RING_BUFFER_ENTRIES - 1;

	tx->ring_buffer_entries_count = RING_BUFFER_ENTRIES;
	tx->ring_buffer_entries_addr = (uintptr_t) entries;
}

void mppa_pcie_eth_init(int if_count)
{
	unsigned int i;
	struct mppa_pcie_eth_ring_buff_desc *desc_ptr;
	g_if_count = if_count;
	eth_control.if_count = g_if_count;

	for (i = 0; i < g_if_count; i++) {
		eth_control.configs[i].mtu = MPPA_PCIE_ETH_DEFAULT_MTU;
		eth_control.configs[i].link_status = 0;
		eth_control.configs[i].mac_addr[5] = '0' + i;
		memcpy(eth_control.configs[i].mac_addr, "\x02\xde\xad\xbe\xef", 5);
		eth_control.configs[i].interrupt_status = 1;

		desc_ptr = calloc(2, sizeof(struct mppa_pcie_eth_ring_buff_desc));
		if(!desc_ptr)
			assert(0);

		setup_rx(&desc_ptr[0]);
		setup_tx(&desc_ptr[1]);

		g_rx[i] = &desc_ptr[0];
		g_tx[i] = &desc_ptr[1];

		eth_control.configs[i].rx_ring_buf_desc_addr = (uintptr_t) &desc_ptr[0];
		eth_control.configs[i].tx_ring_buf_desc_addr = (uintptr_t) &desc_ptr[1];

		g_interrupt_flags[i] = 0;
	}

	/* Ensure coherency */
	__k1_mb();

	__builtin_k1_swu(&eth_control.magic, MPPA_PCIE_ETH_CONTROL_STRUCT_MAGIC);

	/* Cross fingers for everything to be setup correctly ! */
	mppa_pcie_send_it_to_host();
}

#define MPPA_PCIE_GET_ETH_VALUE(__mode, __member, __if) __builtin_k1_lwu(&g_ ## __mode[__if]->__member)
#define MPPA_PCIE_ETH_GET_RX_TAIL(__if) MPPA_PCIE_GET_ETH_VALUE(rx, tail, __if)
#define MPPA_PCIE_ETH_GET_TX_TAIL(__if) MPPA_PCIE_GET_ETH_VALUE(tx, tail, __if)
#define MPPA_PCIE_ETH_GET_RX_HEAD(__if) MPPA_PCIE_GET_ETH_VALUE(rx, head, __if)
#define MPPA_PCIE_ETH_GET_TX_HEAD(__if) MPPA_PCIE_GET_ETH_VALUE(tx, head, __if)

#define MPPA_PCIE_SET_ETH_VALUE(__mode, __member, __if, __val) __builtin_k1_swu(&g_ ## __mode[__if]->__member, __val)
#define MPPA_PCIE_ETH_SET_RX_TAIL(__if, __val) MPPA_PCIE_SET_ETH_VALUE(rx, tail, __if, __val)
#define MPPA_PCIE_ETH_SET_TX_TAIL(__if, __val) MPPA_PCIE_SET_ETH_VALUE(tx, tail, __if, __val)
#define MPPA_PCIE_ETH_SET_RX_HEAD(__if, __val) MPPA_PCIE_SET_ETH_VALUE(rx, head, __if, __val)
#define MPPA_PCIE_ETH_SET_TX_HEAD(__if, __val) MPPA_PCIE_SET_ETH_VALUE(tx, head, __if, __val)

#define MPPA_PCIE_ETH_SET_ENTRY_VALUE(__entry, __memb, __val) __builtin_k1_swu(&__entry->__memb, __val)
#define MPPA_PCIE_ETH_SET_DENTRY_VALUE(__entry, __memb, __val) __builtin_k1_sdu(&__entry->__memb, __val)
#define MPPA_PCIE_ETH_SET_ENTRY_LEN(__entry, __val) MPPA_PCIE_ETH_SET_ENTRY_VALUE(__entry, len, __val)
#define MPPA_PCIE_ETH_SET_ENTRY_ADDR(__entry, __val) MPPA_PCIE_ETH_SET_DENTRY_VALUE(__entry, pkt_addr, __val)

int mppa_pcie_eth_enqueue_tx(unsigned int if_id, void *addr, unsigned int size)
{
	unsigned int rx_tail = MPPA_PCIE_ETH_GET_RX_TAIL(if_id);
	unsigned int rx_head = MPPA_PCIE_ETH_GET_RX_HEAD(if_id);
	struct mppa_pcie_eth_rx_ring_buff_entry *entry, **entries;
	uint64_t daddr = (uintptr_t) addr;

	/* Check if there is room to send a packet to host */
	rx_tail = (rx_tail + 1) % RING_BUFFER_ENTRIES;
	if (rx_tail == rx_head)
		return -1;

	entries = (void *) (uintptr_t) g_rx[if_id]->ring_buffer_entries_addr;
	entry = entries[rx_tail];

	MPPA_PCIE_ETH_SET_ENTRY_LEN(entry, size);
	MPPA_PCIE_ETH_SET_ENTRY_ADDR(entry, daddr);

	MPPA_PCIE_ETH_SET_RX_TAIL(if_id, rx_tail);
	mppa_pcie_send_it_to_host();

	return 0;
}

int mppa_pcie_eth_enqueue_rx(unsigned int if_id, void *addr, unsigned int size)
{
	unsigned int tx_tail = MPPA_PCIE_ETH_GET_TX_TAIL(if_id);
	unsigned int tx_head = MPPA_PCIE_ETH_GET_TX_HEAD(if_id);
	struct mppa_pcie_eth_tx_ring_buff_entry *entry, **entries;
	uint64_t daddr = (uintptr_t) addr;

	/* Check if there is room to add a rx */
	tx_head = (tx_head + 1) % RING_BUFFER_ENTRIES;
	if (tx_head == tx_tail)
		return -1;

	entries = (void *) (uintptr_t) g_tx[if_id]->ring_buffer_entries_addr;
	entry = entries[tx_head];

	MPPA_PCIE_ETH_SET_ENTRY_LEN(entry, size);
	MPPA_PCIE_ETH_SET_ENTRY_ADDR(entry, daddr);

	MPPA_PCIE_ETH_SET_TX_HEAD(if_id, tx_head);
	mppa_pcie_send_it_to_host();

	return 0;
}
