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
void main_loop(int n_if, int drop_all)
{
	struct mppa_pcie_eth_h2c_ring_buff_entry *h2c_entry, *h2c_entries;
	struct mppa_pcie_eth_c2h_ring_buff_entry *c2h_entry, *c2h_entries;
	uint64_t tmp;
	uint32_t h2c_head, c2h_tail, h2c_tail, len;
	int i;

	for (i = 0; i < n_if; i++) {
		int src_if = i;
		int dst_if = i + 1;
		if (dst_if >= n_if)
			dst_if -= n_if;

		/* Handle incoming h2c packet */
		struct mppa_pcie_eth_ring_buff_desc * h2c_rbuf =
			netdev_get_h2c_ring_buffer(src_if);
		struct mppa_pcie_eth_ring_buff_desc * c2h_rbuf =
			netdev_get_c2h_ring_buffer(dst_if);

		h2c_head = __builtin_k1_lwu(&h2c_rbuf->head);
		h2c_tail = __builtin_k1_lwu(&h2c_rbuf->tail);

		if (h2c_head != h2c_tail) {
			if (drop_all) {
				h2c_head = (h2c_head + 1) % RING_BUFFER_ENTRIES;
				__builtin_k1_swu(&h2c_rbuf->head, h2c_head);
				continue;
			}
			h2c_entries = (void *) (uintptr_t) h2c_rbuf->ring_buffer_entries_addr;
			c2h_entries = (void *) (uintptr_t) c2h_rbuf->ring_buffer_entries_addr;

			c2h_tail = __builtin_k1_lwu(&c2h_rbuf->tail);

			h2c_entry = &h2c_entries[h2c_head];
			c2h_entry = &c2h_entries[c2h_tail];
			len = __builtin_k1_lwu(&h2c_entry->len);
#ifdef VERBOSE
			printf("Received packet from host on interface %d, size %"PRIu32", index %"PRIu32"\n",
			       i, len, h2c_head);
#endif

			c2h_tail = (c2h_tail + 1) % RING_BUFFER_ENTRIES;

			/* If the user head is the same as our next tail, there is no room to store a packet */
			while(c2h_tail == __builtin_k1_lwu(&c2h_rbuf->head)) {}

			__builtin_k1_swu(&c2h_entry->len, len);
			/* Swap buffers */
			tmp = __builtin_k1_ldu(&c2h_entry->pkt_addr);
			__builtin_k1_sdu(&c2h_entry->pkt_addr, __builtin_k1_ldu(&h2c_entry->pkt_addr));
			__builtin_k1_sdu(&h2c_entry->pkt_addr, tmp);

			/* Update h2c head and c2h tail pointer and send it */
			__builtin_k1_swu(&c2h_rbuf->tail, c2h_tail);
			h2c_head = (h2c_head + 1) % RING_BUFFER_ENTRIES;
			__builtin_k1_swu(&h2c_rbuf->head, h2c_head);

#ifdef VERBOSE
			printf("New c2h tail : %"PRIu32", h2c head: %"PRIu32"\n", c2h_tail, h2c_head);
#endif
			if (__builtin_k1_lwu(&eth_control.configs[dst_if].interrupt_status))
				mppa_pcie_send_it_to_host();
		}
	}
}

static eth_if_cfg_t if_cfgs[IF_COUNT] = {
	[ 0 ... IF_COUNT - 1] =
	{ .if_id = 0, .mtu = MPPA_PCIE_ETH_DEFAULT_MTU,
	  .n_c2h_entries = RING_BUFFER_ENTRIES,
	  .n_h2c_entries = RING_BUFFER_ENTRIES,
	  .mac_addr = { 0xde, 0xad, 0xbe, 0xef },
	  .flags = 0,
	}
};


int main(int argc, char* argv[])
{
	int opt;

	unsigned n_if = 1;
	unsigned drop_all = 0;
	while ((opt = getopt(argc, argv, "dn:")) != -1) {
		switch (opt) {
		case 'd':
			drop_all = 1;
			break;
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
		main_loop(n_if, drop_all);
	}

	return 0;
}
