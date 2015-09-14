#ifndef MPPA_PCIE_NOC_H
#define MPPA_PCIE_NOC_H

#include "odp_rpc_internal.h"
#include "rpc-server.h"

int mppa_pcie_eth_noc_init();
int mppa_pcie_eth_setup_tx(unsigned int iface_id,unsigned int tx_id, unsigned int cluster_id, unsigned int rx_id);
volatile void *mppa_pcie_eth_get_fifo_addr(int iface_id, int tx_id);
odp_rpc_cmd_ack_t mppa_pcie_eth_open(unsigned remoteClus, odp_rpc_t * msg);

#endif
