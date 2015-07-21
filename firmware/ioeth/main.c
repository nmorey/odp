#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include <HAL/hal/hal.h>

#ifndef BSP_NB_DMA_IO_MAX
#define BSP_NB_DMA_IO_MAX 1
#endif

#include <mppa_bsp.h>
#include <mppa_routing.h>
#include <mppa_noc.h>
#include "odp_rpc_internal.h"
#include "eth.h"

#define RPC_PKT_SIZE (sizeof(odp_rpc_t) + RPC_MAX_PAYLOAD)

static struct {
	uint32_t recv_pkt_count;
	void    *recv_buf;
} g_clus_priv[BSP_NB_CLUSTER_MAX];


odp_rpc_cmd_ack_t rpcHandle(unsigned remoteClus, odp_rpc_t * msg)
{

	(void)remoteClus;
	switch (msg->pkt_type){
	case ODP_RPC_CMD_OPEN:
		return eth_open_rx(remoteClus, msg);
		break;
	case ODP_RPC_CMD_CLOS:
		return eth_close_rx(remoteClus, msg);
		break;
	case ODP_RPC_CMD_INVL:
		break;
	default:
		fprintf(stderr, "Invalid MSG\n");
		exit(EXIT_FAILURE);
	}
	odp_rpc_cmd_ack_t ack = {.status = -1 };
	return ack;
}

static int cluster_init_dnoc_rx(int clus_id)
{
	mppa_noc_ret_t ret;
	mppa_noc_dnoc_rx_configuration_t conf = {0};

	g_clus_priv[clus_id].recv_buf = malloc(RPC_PKT_SIZE);
	if (!g_clus_priv[clus_id].recv_buf)
		return 1;

	/* DNoC */
	ret = mppa_noc_dnoc_rx_alloc(clus_id / 4, RPC_BASE_RX + clus_id % 4);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	conf.buffer_base = (uintptr_t)g_clus_priv[clus_id].recv_buf;
	conf.buffer_size = RPC_PKT_SIZE;
	conf.activation = MPPA_NOC_ACTIVATED;
	conf.reload_mode = MPPA_NOC_RX_RELOAD_MODE_INCR_DATA_NOTIF;

	ret = mppa_noc_dnoc_rx_configure(clus_id / 4, RPC_BASE_RX + clus_id % 4, conf);
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

static int ack_msg(odp_rpc_t * msg, odp_rpc_cmd_ack_t ack)
{
	msg->ack = 1;
	msg->data_len = 0;
	msg->inl_data = ack.inl_data;

	return odp_rpc_send_msg(msg->dma_id / 4, msg->dma_id, msg->dnoc_tag, msg, NULL);
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
			int remoteClus = 4 * if_id + (tag - RPC_BASE_RX);
			odp_rpc_t *msg = g_clus_priv[remoteClus].recv_buf;

			odp_rpc_cmd_ack_t ack = rpcHandle(remoteClus, msg);

			/* Ack the RPC message */
			ack_msg(msg, ack);
		}
	}
	return 0;
}
