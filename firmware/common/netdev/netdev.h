#ifndef NETDEV__H
#define NETDEV__H

#include <mppa_pcie_netdev.h>

#define IF_COUNT		1
#define RING_BUFFER_ENTRIES	32

extern struct mppa_pcie_eth_ring_buff_desc *g_netdev_rx[];
extern struct mppa_pcie_eth_ring_buff_desc *g_netdev_tx[];

int netdev_init_configs();

#endif /* NETDEV__H */
