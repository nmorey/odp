#include <stdio.h>
#include <string.h>
#include <mppa_noc.h>
#include <mppa_routing.h>
#include "mppa_pcie_noc.h"
#include "mppa_pcie_eth.h"
#include "HAL/hal/hal.h"

#define DDR_BUFFER_BASE_ADDR		0x80000000

#define MPPA_PCIE_ETH_NOC_PKT_COUNT	16

static unsigned int g_pkt_base_addr = DDR_BUFFER_BASE_ADDR;

struct mppa_pcie_eth_dnoc_tx_cfg g_mppa_pcie_tx_cfg[BSP_NB_IOCLUSTER_MAX][BSP_DNOC_TX_PACKETSHAPER_NB_MAX] = {{{0}}};

int mppa_pcie_eth_noc_init()
{
	int i;

	for(i = 0; i < BSP_NB_DMA_IO_MAX; i++)
		mppa_noc_interrupt_line_disable(i, MPPA_NOC_INTERRUPT_LINE_DNOC_TX);

	return 0;
}

static int mppa_pcie_eth_setup_tx(unsigned int iface_id, unsigned int *tx_id, unsigned int cluster_id, unsigned int rx_id)
{
	mppa_noc_ret_t nret;
	mppa_routing_ret_t rret;
	mppa_dnoc_header_t header;
	mppa_dnoc_channel_config_t config;

	/* Configure the TX for PCIe */
	nret = mppa_noc_dnoc_tx_alloc_auto(iface_id, tx_id, MPPA_NOC_NON_BLOCKING);
	if (nret) {
		printf("Tx alloc failed\n");
		return 1;
	}

	MPPA_NOC_DNOC_TX_CONFIG_INITIALIZER_DEFAULT(config, 0);

	rret = mppa_routing_get_dnoc_unicast_route(__k1_get_cluster_id() + iface_id, cluster_id, &config, &header);
	if (rret) {
		printf("Routing failed\n");
		return 1;
	}

	header._.tag = rx_id;
	header._.valid = 1;

	nret = mppa_noc_dnoc_tx_configure(iface_id, *tx_id, header, config);
	if (nret) {
		printf("Tx configure failed\n");
		return 1;
	}

	return 0;
}

static int mppa_pcie_eth_setup_rx(int if_id, unsigned int *rx_id)
{
	unsigned int buf_size = MPPA_PCIE_ETH_DEFAULT_MTU * MPPA_PCIE_ETH_NOC_PKT_COUNT;
	mppa_noc_ret_t ret;
	mppa_noc_dnoc_rx_configuration_t conf = MPPA_NOC_DNOC_RX_CONFIGURATION_INIT;

	ret = mppa_noc_dnoc_rx_alloc_auto(if_id, rx_id, MPPA_NOC_NON_BLOCKING);
	if(ret) {
		fprintf(stderr, "[PCIe] Error: Failed to find an available Rx on if %d\n", if_id);
		return 1;
	}

	conf.buffer_base = g_pkt_base_addr;
	conf.buffer_size = buf_size;
	g_pkt_base_addr += buf_size;

	conf.reload_mode = MPPA_NOC_RX_RELOAD_MODE_INCR_DATA_NOTIF;
	conf.activation = MPPA_NOC_ACTIVATED;

	ret = mppa_noc_dnoc_rx_configure(if_id, *rx_id, conf);
	if (ret)
		return 1;

	return 0;
}

odp_rpc_cmd_ack_t mppa_pcie_eth_open(unsigned remoteClus, odp_rpc_t * msg)
{
	odp_rpc_cmd_pcie_open_t open_cmd = {.inl_data = msg->inl_data};
	odp_rpc_cmd_ack_t ack = ODP_RPC_CMD_ACK_INITIALIZER;
	struct mppa_pcie_eth_dnoc_tx_cfg *tx_cfg;
	int if_id = remoteClus % 4;
	unsigned int tx_id, rx_id;

	printf("Received request to open PCIe\n");
	int ret = mppa_pcie_eth_setup_tx(if_id, &tx_id, remoteClus, open_cmd.min_rx);
	if (ret) {
		fprintf(stderr, "[PCIe] Error: Failed to setup tx on if %d\n", if_id);
		return ack;
	}

	ret = mppa_pcie_eth_setup_rx(if_id, &rx_id);
	if (ret)
		return ack;

	tx_cfg = &g_mppa_pcie_tx_cfg[if_id][tx_id];
	tx_cfg->opened = 1; 
	tx_cfg->cluster = remoteClus;
	tx_cfg->rx_id = rx_id;
	tx_cfg->fifo_addr = &mppa_dnoc[if_id]->tx_ports[tx_id].push_data;
	tx_cfg->pcie_eth_if = open_cmd.pcie_eth_if_id; 
	tx_cfg->mtu = open_cmd.pkt_size;

	ret = mppa_pcie_eth_add_forward(open_cmd.pcie_eth_if_id, &g_mppa_pcie_tx_cfg[if_id][tx_id]);
	if (ret)
		return ack;

	ack.cmd.pcie_open.tx_tag = rx_id;
	ack.cmd.pcie_open.tx_if = __k1_get_cluster_id() + if_id;
	/* FIXME, we send the same MTU as the one received */
	ack.cmd.pcie_open.mtu = open_cmd.pkt_size;
	memcpy(ack.cmd.pcie_open.mac, g_pcie_eth_control.configs[open_cmd.pcie_eth_if_id].mac_addr, MAC_ADDR_LEN);
	ack.status = 0;

	return ack;
}

odp_rpc_cmd_ack_t mppa_pcie_eth_close(__attribute__((unused)) unsigned remoteClus, __attribute__((unused)) odp_rpc_t * msg)
{
	odp_rpc_cmd_ack_t ack = ODP_RPC_CMD_ACK_INITIALIZER;
	ack.status = 0;

	return ack;
}
