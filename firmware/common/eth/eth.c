#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include <HAL/hal/hal.h>

#include <odp_rpc_internal.h>
#include <mppa_eth_core.h>
#include <mppa_eth_loadbalancer_core.h>
#include <mppa_eth_phy.h>
#include <mppa_eth_mac.h>
#include <mppa_routing.h>
#include <mppa_noc.h>

#include "io_utils.h"
#include "rpc-server.h"
#include "eth.h"

eth_status_t status[N_ETH_LANE];

static inline int get_eth_dma_id(unsigned cluster_id){
	unsigned offset = cluster_id / 4;
#ifdef K1B_EXPLORER
	offset = 0;
#endif

	switch(__k1_get_cluster_id()){
#ifndef K1B_EXPLORER
	case 128:
	case 160:
		return offset + 4;
#endif
	case 192:
	case 224:
		return offset + 4;
	default:
		return -1;
	}
}

odp_rpc_cmd_ack_t  eth_open(unsigned remoteClus, odp_rpc_t *msg)
{
	odp_rpc_cmd_ack_t ack = { .status = 0};
	odp_rpc_cmd_eth_open_t data = { .inl_data = msg->inl_data };
	const int nocIf = get_eth_dma_id(remoteClus);
	const unsigned int eth_if = data.ifId % 4; /* 4 is actually 0 in 40G mode */

	if(nocIf < 0) {
		fprintf(stderr, "[ETH] Error: Invalid NoC interface (%d %d)\n", nocIf, remoteClus);
		goto err;
	}

	if(eth_if >= N_ETH_LANE) {
		fprintf(stderr, "[ETH] Error: Invalid Eth lane\n");
		goto err;
	}

	if(status[eth_if].cluster[remoteClus].txId >= 0) {
		fprintf(stderr, "[ETH] Error: Lane %d is already opened for cluster %d\n",
			eth_if, remoteClus);
		goto err;
	}

	int externalAddress = __k1_get_cluster_id() + nocIf;
#ifdef K1B_EXPLORER
	externalAddress = __k1_get_cluster_id() + (nocIf % 4);
#endif

	status[eth_if].cluster[remoteClus].rx_enabled = data.rx_enabled;
	status[eth_if].cluster[remoteClus].tx_enabled = data.tx_enabled;

	if (ethtool_setup_eth2clus(remoteClus, eth_if, nocIf, externalAddress,
				   data.min_rx, data.max_rx))
		goto err;
	if (ethtool_setup_clus2eth(remoteClus, eth_if, nocIf))
		goto err;
	if (ethtool_init_lane(eth_if, data.loopback))
		goto err;
	if (ethtool_enable_cluster(remoteClus, eth_if))
		goto err;

	ack.cmd.eth_open.tx_if = externalAddress;
	ack.cmd.eth_open.tx_tag = status[eth_if].cluster[remoteClus].rx_tag;
	ack.cmd.eth_open.mtu = 1500;
	memset(ack.cmd.eth_open.mac, 0, ETH_ALEN);
	ack.cmd.eth_open.mac[ETH_ALEN-1] = 1 << eth_if;
	ack.cmd.eth_open.mac[ETH_ALEN-2] = __k1_get_cluster_id();

	return ack;

 err:
	ethtool_cleanup_cluster(remoteClus, eth_if);
	ack.status = 1;
	return ack;
}

odp_rpc_cmd_ack_t  eth_close(unsigned remoteClus, odp_rpc_t *msg)
{
	odp_rpc_cmd_ack_t ack = { .status = 0 };
	odp_rpc_cmd_eth_clos_t data = { .inl_data = msg->inl_data };
	const int nocTx = status[data.ifId].cluster[remoteClus].txId;
	const unsigned int eth_if = data.ifId % 4; /* 4 is actually 0 in 40G mode */

	if(nocTx < 0) {
		ack.status = -1;
		return ack;
	}

	ethtool_disable_cluster(remoteClus, eth_if);
	ethtool_cleanup_cluster(remoteClus, eth_if);

	return ack;
}

static void eth_init(void)
{
	/* "MATCH_ALL" Rule */
	mppabeth_lb_cfg_rule((void *)&(mppa_ethernet[0]->lb),
			     ETH_MATCHALL_TABLE_ID, ETH_MATCHALL_RULE_ID,
			     /* offset */ 0, /* Cmp Mask */0,
			     /* Espected Value */ 0, /* Hash. Unused */0);

	mppabeth_lb_cfg_extract_table_mode((void *)&(mppa_ethernet[0]->lb),
					   ETH_MATCHALL_TABLE_ID, /* Priority */ 0,
					   MPPABETHLB_DISPATCH_POLICY_RR);
	for (int eth_if = 0; eth_if < N_ETH_LANE; ++eth_if) {
		_eth_status_init(&status[eth_if]);

		mppabeth_lb_cfg_header_mode((void *)&(mppa_ethernet[0]->lb),
					    eth_if, MPPABETHLB_ADD_HEADER);

		mppabeth_lb_cfg_table_rr_dispatch_trigger((void *)&(mppa_ethernet[0]->lb),
							  ETH_MATCHALL_TABLE_ID,
							  eth_if, 1);

	}
}

static int eth_rpc_handler(unsigned remoteClus, odp_rpc_t *msg, uint8_t *payload)
{
	odp_rpc_cmd_ack_t ack = ODP_RPC_CMD_ACK_INITIALIZER;

	(void)payload;
	switch (msg->pkt_type){
	case ODP_RPC_CMD_ETH_OPEN:
		ack = eth_open(remoteClus, msg);
		break;
	case ODP_RPC_CMD_ETH_CLOS:
		ack = eth_close(remoteClus, msg);
		break;
	default:
		return -1;
	}
	odp_rpc_server_ack(msg, ack);
	return 0;
}

void  __attribute__ ((constructor)) __eth_rpc_constructor()
{
	eth_init();
	if(__n_rpc_handlers < MAX_RPC_HANDLERS) {
		__rpc_handlers[__n_rpc_handlers++] = eth_rpc_handler;
	} else {
		fprintf(stderr, "Failed to register ETH RPC handlers\n");
		exit(EXIT_FAILURE);
	}
}
