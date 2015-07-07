#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include <HAL/hal/hal.h>
#include <mppa_ethernet_shared.h>

#include <mppa_bsp.h>
#include <mppa_routing.h>
#include <mppa_noc.h>
#include "rpc.h"

#define RPC_PKT_SIZE (sizeof(odp_rpc_t) + RPC_MAX_PAYLOAD)

static uint8_t *g_pkt_recv_buf[BSP_NB_CLUSTER_MAX];

static int cluster_init_dnoc_rx(int clus_id)
{
	mppa_noc_ret_t ret;
	mppa_noc_dnoc_rx_configuration_t conf = {0};

	g_pkt_recv_buf[clus_id] = malloc(RPC_PKT_SIZE);
	if (g_pkt_recv_buf[clus_id] == NULL)
		return 1;

	/* DNoC */
	ret = mppa_noc_dnoc_rx_alloc(clus_id % 4, RPC_BASE_RX + clus_id % 4);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	conf.buffer_base = (uintptr_t) g_pkt_recv_buf[clus_id];
	conf.buffer_size = RPC_PKT_SIZE;
	conf.activation = MPPA_NOC_ACTIVATED;
	conf.reload_mode = MPPA_NOC_RX_RELOAD_MODE_INCR_DATA_NOTIF;

	ret = mppa_noc_dnoc_rx_configure(clus_id % 4, RPC_BASE_RX + clus_id % 4, conf);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	return 0;
}

int main(){
	int ret;

	for(int i = 0; i < BSP_NB_CLUSTER_MAX; ++i){
		ret = cluster_init_dnoc_rx(i);
		if (ret != 0) {
			exit(EXIT_FAILURE);
		}
	}

	return 0;
}
