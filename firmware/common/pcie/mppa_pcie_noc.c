#include <stdio.h>
#include <mppa_noc.h>
#include <mppa_routing.h>
#include "mppa_pcie_noc.h"
#include "HAL/hal/hal.h"

int mppa_pcie_eth_noc_init()
{
	int i;

	for(i = 0; i < BSP_NB_DMA_IO_MAX; i++)
		mppa_noc_interrupt_line_disable(i, MPPA_NOC_INTERRUPT_LINE_DNOC_TX);

	return 0;
}

int mppa_pcie_eth_setup_tx(unsigned int iface_id, unsigned int tx_id, unsigned int cluster_id, unsigned int rx_id)
{
	mppa_noc_ret_t nret;
	mppa_routing_ret_t rret;
	mppa_dnoc_header_t header;
	mppa_dnoc_channel_config_t config;

	/* Configure the TX for PCIe */
	nret = mppa_noc_dnoc_tx_alloc(iface_id, tx_id);
	if (nret)
		return 1;

	MPPA_NOC_DNOC_TX_CONFIG_INITIALIZER_DEFAULT(config, 0);

	rret = mppa_routing_get_dnoc_unicast_route(__k1_get_cluster_id() + iface_id, cluster_id, &config, &header);
	if (rret)
		return 1;

	header._.tag = rx_id;
	header._.valid = 1;

	nret = mppa_noc_dnoc_tx_configure(iface_id, tx_id, header, config);
	if (nret)
		return 1;

	return 0;
}

volatile void *mppa_pcie_eth_get_fifo_addr(int iface_id, int tx_id)
{
	return &mppa_dnoc[iface_id]->dma_pcie_fifo.dma_rx[tx_id].pcie_fifo;
}

odp_rpc_cmd_ack_t mppa_pcie_eth_open(unsigned remoteClus, odp_rpc_t * msg)
{
	odp_rpc_cmd_pcie_open_t open_cmd = {.inl_data = msg->inl_data};
	odp_rpc_cmd_ack_t ack = {.status = -1};
	int if_id = remoteClus % BSP_NB_DMA_IO_MAX;
	int tx_id = remoteClus / BSP_NB_DMA_IO_MAX;
	unsigned int rx_id;

	printf("Received request to open PCIe\n");
	int ret = mppa_pcie_eth_setup_tx(if_id, tx_id, remoteClus,  open_cmd.min_rx);
	if (ret) {
		fprintf(stderr, "[PCIe] Error: Failed to setup tx on if %d\n", if_id);
		return ack;
	}

	ret = mppa_noc_dnoc_rx_alloc_auto(if_id, &rx_id, MPPA_NOC_NON_BLOCKING);
	if(ret) {
		fprintf(stderr, "[PCIe] Error: Failed to find an available Rx on if %d\n", if_id);
		return ack;
	}

	ack.cmd.pcie_open.tx_tag = rx_id;
	ack.cmd.pcie_open.tx_if = __k1_get_cluster_id() + if_id;

	return ack;
}
