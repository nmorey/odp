#ifndef MPPA_PCIE_NOC_H
#define MPPA_PCIE_NOC_H

#include "odp_rpc_internal.h"
#include "rpc-server.h"
#include "mppa_pcie_buf_alloc.h"

#define MPPA_PCIE_ETH_IF_MAX 4

#define MPPA_PCIE_RM_COUNT		4

#define RX_RM_START		1
#define RX_RM_COUNT		2

#define MPPA_PCIE_USABLE_DNOC_IF	4

/**
 * PKT size
 */
#define MPPA_PCIE_MULTIBUF_PKT_SIZE	(9*1024)
/**
 * 4 packets per multi buffer
 */
#define MPPA_PCIE_MULTIBUF_PKT_COUNT	4
#define MPPA_PCIE_MULTIBUF_SIZE		(MPPA_PCIE_MULTIBUF_PKT_COUNT * MPPA_PCIE_MULTIBUF_PKT_SIZE)

#define MPPA_PCIE_MULTIBUF_COUNT	64

#define RX_RM_STACK_SIZE	(0x2000 / (sizeof(uint64_t)))

extern buffer_ring_t g_free_buf_pool;
extern buffer_ring_t g_full_buf_pool;

struct mppa_pcie_eth_dnoc_tx_cfg {
	int opened;
	unsigned int cluster;
	unsigned int mtu;
	volatile void *fifo_addr;
	unsigned int pcie_eth_if;
};

void
mppa_pcie_noc_start_rx_rm();

int mppa_pcie_eth_init(int if_count);
int mppa_pcie_eth_noc_init();

odp_rpc_cmd_ack_t mppa_pcie_eth_open(unsigned remoteClus, odp_rpc_t * msg);
odp_rpc_cmd_ack_t mppa_pcie_eth_close(unsigned remoteClus, odp_rpc_t * msg);

int mppa_pcie_eth_handler();
int mppa_pcie_eth_add_forward(unsigned int pcie_eth_if_id, struct mppa_pcie_eth_dnoc_tx_cfg *dnoc_tx_cfg);
extern struct mppa_pcie_eth_control g_pcie_eth_control;

int mppa_pcie_eth_setup_rx(int if_id, unsigned int *rx_id);

#endif
