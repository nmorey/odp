#ifndef MPPA_PCIE_NOC_H
#define MPPA_PCIE_NOC_H

#include "odp_rpc_internal.h"
#include "rpc-server.h"

struct mppa_pcie_eth_dnoc_tx_cfg {
	int opened;
	unsigned int cluster;
	unsigned int rx_id;
	unsigned int mtu;
	volatile void *fifo_addr;
	unsigned int pcie_eth_if;
};

int mppa_pcie_eth_init(int if_count);
int mppa_pcie_eth_noc_init();
odp_rpc_cmd_ack_t mppa_pcie_eth_open(unsigned remoteClus, odp_rpc_t * msg);

int mppa_pcie_eth_add_forward(unsigned int pcie_eth_if_id, struct mppa_pcie_eth_dnoc_tx_cfg *dnoc_tx_cfg);
extern struct mppa_pcie_eth_control g_pcie_eth_control;

#endif
