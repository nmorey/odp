#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <getopt.h>

#include <mppa/osconfig.h>
#include <HAL/hal/hal.h>

#include "netdev.h"

#define IF_COUNT		8
#define RING_BUFFER_ENTRIES	32

/**
 * Transfer all packet received on host tx to host rx
 */
void main_loop(int n_if)
{
	struct mppa_pcie_eth_tx_ring_buff_entry *tx_entry, *tx_entries;
	struct mppa_pcie_eth_rx_ring_buff_entry *rx_entry, *rx_entries;
	uint64_t tmp;
	uint32_t tx_head, rx_tail, tx_tail, len;
	int i;

	for (i = 0; i < n_if; i++) {
		int src_if = i;
		int dst_if = i + 1;
		if (dst_if >= n_if)
			dst_if -= n_if;

		/* Handle incoming tx packet */
		struct mppa_pcie_eth_ring_buff_desc * tx_rbuf =
			netdev_get_tx_ring_buffer(src_if);
		struct mppa_pcie_eth_ring_buff_desc * rx_rbuf =
			netdev_get_rx_ring_buffer(dst_if);

		tx_head = __builtin_k1_lwu(&tx_rbuf->head);
		tx_tail = __builtin_k1_lwu(&tx_rbuf->tail);

		if (tx_head != tx_tail) {
			tx_entries = (void *) (uintptr_t) tx_rbuf->ring_buffer_entries_addr;
			rx_entries = (void *) (uintptr_t) rx_rbuf->ring_buffer_entries_addr;

			rx_tail = __builtin_k1_lwu(&rx_rbuf->tail);

			tx_entry = &tx_entries[tx_head];
			rx_entry = &rx_entries[rx_tail];
			len = __builtin_k1_lwu(&tx_entry->len);
#ifdef VERBOSE
			printf("Received packet from host on interface %d, size %"PRIu32", index %"PRIu32"\n", i, len, tx_head);
#endif

			rx_tail = (rx_tail + 1) % RING_BUFFER_ENTRIES;

			/* If the user head is the same as our next tail, there is no room to store a packet */
			while(rx_tail == __builtin_k1_lwu(&rx_rbuf->head)) {
			}

			__builtin_k1_swu(&rx_entry->len, len);
			/* Swap buffers */
			tmp = __builtin_k1_ldu(&rx_entry->pkt_addr);
			__builtin_k1_sdu(&rx_entry->pkt_addr, __builtin_k1_ldu(&tx_entry->pkt_addr));
			__builtin_k1_sdu(&tx_entry->pkt_addr, tmp);

			/* Update tx head and rx tail pointer and send it */
			__builtin_k1_swu(&rx_rbuf->tail, rx_tail);
			tx_head = (tx_head + 1) % RING_BUFFER_ENTRIES;
			__builtin_k1_swu(&tx_rbuf->head, tx_head);

#ifdef VERBOSE
			printf("New rx tail : %"PRIu32", tx head: %"PRIu32"\n", rx_tail, tx_head);
#endif
			mppa_pcie_send_it_to_host();
		}
	}
}

static eth_if_cfg_t if_cfgs[IF_COUNT] = {
	[ 0 ... IF_COUNT - 1] =
	{ .if_id = 0, .mtu = MPPA_PCIE_ETH_DEFAULT_MTU,
	  .n_rx_entries = RING_BUFFER_ENTRIES,
	  .n_tx_entries = RING_BUFFER_ENTRIES,
	  .mac_addr = { 0xde, 0xad, 0xbe, 0xef }
	}
};


int main(int argc, char* argv[])
{
	int opt;

	unsigned n_if = 1;

	while ((opt = getopt(argc, argv, "n:")) != -1) {
		switch (opt) {
		case 'n':
			n_if = atoi(optarg);
			break;
		default: /* '?' */
			fprintf(stderr, "Wrong arguments for boot\n");
			return -1;
		}
	}
	printf("Starting %u interfaces\n", n_if);

	for (int i = 0; i < IF_COUNT; ++i){
		if_cfgs[i].if_id = i;
		if_cfgs[i].mac_addr[MAC_ADDR_LEN - 1] = i;
	}

	netdev_init(n_if, if_cfgs);
	netdev_start();


	while(1) {
		main_loop(n_if);
	}

	return 0;
}
