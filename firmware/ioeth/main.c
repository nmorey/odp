#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include <HAL/hal/hal.h>

#include <mppa_bsp.h>
#include <mppa_routing.h>
#include <mppa_noc.h>
#include "rpc.h"
#include "eth.h"

#define RPC_PKT_SIZE (sizeof(odp_rpc_t) + RPC_MAX_PAYLOAD)

static struct {
	uint32_t recv_pkt_count;
	void    *recv_buf;
} g_clus_priv[BSP_NB_CLUSTER_MAX];

static int cluster_init_dnoc_rx(int clus_id)
{
	mppa_noc_ret_t ret;
	mppa_noc_dnoc_rx_configuration_t conf = {0};

	g_clus_priv[clus_id].recv_buf = malloc(RPC_PKT_SIZE);
	if (!g_clus_priv[clus_id].recv_buf)
		return 1;

	/* DNoC */
	ret = mppa_noc_dnoc_rx_alloc(clus_id % 4, RPC_BASE_RX + clus_id / 4);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	conf.buffer_base = (uintptr_t)g_clus_priv[clus_id].recv_buf;
	conf.buffer_size = RPC_PKT_SIZE;
	conf.activation = MPPA_NOC_ACTIVATED;
	conf.reload_mode = MPPA_NOC_RX_RELOAD_MODE_INCR_DATA_NOTIF;

	ret = mppa_noc_dnoc_rx_configure(clus_id % 4, RPC_BASE_RX + clus_id / 4, conf);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	return 0;
}

static int get_if_rx_id(unsigned interface_id)
{
	for (int i = 0; i < 3; ++i) {
		uint64_t mask = mppa_dnoc[interface_id]->rx_global.events[i].dword;
		if (mask) {
			return __k1_ctzdl(mask) + i * 8 * sizeof(mask);
		}
	}
	return -1;
}

static int ack_msg(int clus_id, odp_rpc_t * msg)
{
	mppa_noc_ret_t nret;
	mppa_routing_ret_t rret;
	mppa_cnoc_config_t config;
	mppa_cnoc_header_t header;

	rret = mppa_routing_get_cnoc_unicast_route(__k1_get_cluster_id(),
						   msg->dma_id,
						   &config, &header);
	if (rret != MPPA_ROUTING_RET_SUCCESS)
		return 1;

	header._.tag = msg->cnoc_id;

	nret = mppa_noc_cnoc_tx_configure(0, 0, config, header);
	if (nret != MPPA_NOC_RET_SUCCESS)
		return 1;

	mppa_noc_cnoc_tx_push(0, 0, g_clus_priv[clus_id].recv_pkt_count);

	return 0;
}

int main()
{
	int ret;

	for (int i = 0; i < BSP_NB_CLUSTER_MAX; ++i) {
		ret = cluster_init_dnoc_rx(i);
		if (ret != 0) {
			exit(EXIT_FAILURE);
		}
	}
	eth_init();

	while (1) {
		for (int if_id = 0; if_id < 4; ++if_id) {
			int tag = get_if_rx_id(if_id);
			if(tag < 0)
				continue;

			/* Received a message */
			int remoteClus = if_id + 4 * (tag - RPC_BASE_RX);
			odp_rpc_t *msg = g_clus_priv[remoteClus].recv_buf;

			rpcHandle(remoteClus, msg);

			/* Ack the RPC message */
			ack_msg(remoteClus, msg);
		}
	}
	return 0;
}
