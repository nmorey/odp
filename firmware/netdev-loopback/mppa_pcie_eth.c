#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

#include <mppa/osconfig.h>
#include <HAL/hal/hal.h>

#include "netdev.h"

/**
 * Transfer all packet received on host tx to host rx
 */
void main_loop()
{
	struct mppa_pcie_eth_tx_ring_buff_entry *tx_entry, *tx_entries;
	struct mppa_pcie_eth_rx_ring_buff_entry *rx_entry, *rx_entries;
	uint64_t tmp;
	uint32_t tx_head, rx_tail, tx_tail, len;
	int i;

	for (i = 0; i < IF_COUNT; i++) {
		/* Handle incoming tx packet */
		tx_head = __builtin_k1_lwu(&g_netdev_tx[i]->head);
		tx_tail = __builtin_k1_lwu(&g_netdev_tx[i]->tail);

		if (tx_head != tx_tail) {
			tx_entries = (void *) (uintptr_t) g_netdev_tx[i]->ring_buffer_entries_addr;
			rx_entries = (void *) (uintptr_t) g_netdev_rx[i]->ring_buffer_entries_addr;

			rx_tail = __builtin_k1_lwu(&g_netdev_rx[i]->tail);

			tx_entry = &tx_entries[tx_head];
			rx_entry = &rx_entries[rx_tail];
			len = __builtin_k1_lwu(&tx_entry->len);
			printf("Received packet from host on interface %d, size %"PRIu32", index %"PRIu32"\n", i, len, tx_head);

			rx_tail = (rx_tail + 1) % RING_BUFFER_ENTRIES;

			/* If the user head is the same as our next tail, there is no room to store a packet */
			while(rx_tail == __builtin_k1_lwu(&g_netdev_rx[i]->head)) {
			}

			__builtin_k1_swu(&rx_entry->len, __builtin_k1_lwu(&tx_entry->len));
			/* Swap buffers */
			tmp = __builtin_k1_ldu(&rx_entry->pkt_addr);
			__builtin_k1_sdu(&rx_entry->pkt_addr, __builtin_k1_ldu(&tx_entry->pkt_addr));
			__builtin_k1_sdu(&tx_entry->pkt_addr, tmp);

			/* Update tx head and rx tail pointer and send it */
			__builtin_k1_swu(&g_netdev_rx[i]->tail, rx_tail);
			tx_head = (tx_head + 1) % RING_BUFFER_ENTRIES;
			__builtin_k1_swu(&g_netdev_tx[i]->head, tx_head);

			printf("New rx tail : %"PRIu32", tx head: %"PRIu32"\n", rx_tail, tx_head);

			mppa_pcie_send_it_to_host();
		}
	}
}

int main()
{
	netdev_init_configs();

	printf("Waiting for packets\n");

	while(1) {
		main_loop();
	}

	return 0;
}
