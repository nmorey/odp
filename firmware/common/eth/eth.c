#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include <HAL/hal/hal.h>

#include <odp_rpc_internal.h>
#include <libmppa_eth_core.h>
#include <libmppa_eth_loadbalancer_core.h>
#include <libmppa_eth_phy.h>
#include <libmppa_eth_mac.h>
#include <mppa_routing.h>
#include <mppa_noc.h>

#include "io_utils.h"
#include "rpc-server.h"
#include "eth.h"

#ifdef K1B_EXPLORER
#define N_ETH_LANE 1
#else
#define N_ETH_LANE 4
#endif

typedef struct {
	int txId;
	int min_rx;
	int max_rx;

	int rx_tag;
} eth_cluster_status_t;
typedef struct {
	int initialized;
	int laneStatus;
	eth_cluster_status_t cluster[BSP_NB_CLUSTER_MAX];
} eth_status_t;

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

static inline void _eth_cluster_status_init(eth_cluster_status_t * cluster)
{
	cluster->txId = -1;
	cluster->min_rx = 0;
	cluster->max_rx = -1;
	cluster->rx_tag = -1;
}

static inline void _eth_status_init(eth_status_t * status)
{
	status->initialized = 0;
	status->laneStatus = -1;

	for (int i = 0; i < BSP_NB_CLUSTER_MAX; ++i)
		_eth_cluster_status_init(&status->cluster[i]);
}

odp_rpc_cmd_ack_t  eth_open(unsigned remoteClus, odp_rpc_t *msg)
{
	odp_rpc_cmd_ack_t ack = { .status = 0};
	odp_rpc_cmd_open_t data = { .inl_data = msg->inl_data };
	const int nocIf = get_eth_dma_id(remoteClus);
	volatile mppa_dnoc_min_max_task_id_t *context;
	mppa_dnoc_header_t header = { 0 };
	mppa_dnoc_channel_config_t config = { 0 };
	unsigned nocTx;
	int ret;
	const unsigned int eth_if = data.ifId % 4; /* 4 is actually 0 in 40G mode */

	if(nocIf < 0) {
		fprintf(stderr, "[ETH] Error: Invalid NoC interface (%d %d)\n", nocIf, remoteClus);
		goto err;
	}

	if(eth_if >= N_ETH_LANE) {
		fprintf(stderr, "[ETH] Error: Invalid Eth lane\n");
		goto err;
	}

	if(status[eth_if].initialized == 0){
		ret = init_mac(eth_if, eth_if == 4 ? MPPABETHMAC_ETHMODE_40G : -1);
		if(ret) {
			fprintf(stderr, "[ETH] Error: Failed to initialize lane %d (%d)\n", eth_if, ret);
			goto err;
		}
	}

	if(status[eth_if].cluster[remoteClus].txId >= 0) {
		fprintf(stderr, "[ETH] Error: Lane %d is already opened for cluster %d\n",
			eth_if, remoteClus);
		goto err;
	}

	/* Configure Tx */
	int externalAddress = __k1_get_cluster_id() + nocIf;
#ifdef K1B_EXPLORER
	externalAddress = __k1_get_cluster_id() + (nocIf % 4);
#endif
	ret = mppa_routing_get_dnoc_unicast_route(externalAddress,
						  remoteClus, &config, &header);
	if (ret != MPPA_ROUTING_RET_SUCCESS) {
		fprintf(stderr, "[ETH] Error: Failed to route to cluster %d\n", remoteClus);
		goto err;
	}

	ret = mppa_noc_dnoc_tx_alloc_auto(nocIf, &nocTx, MPPA_NOC_BLOCKING);
	if (ret != MPPA_NOC_RET_SUCCESS) {
		fprintf(stderr, "[ETH] Error: Failed to find an available Tx on DMA %d\n", nocIf);
		goto err;
	}

	config._.loopback_multicast = 0;
	config._.cfg_pe_en = 1;
	config._.cfg_user_en = 1;
	config._.write_pe_en = 1;
	config._.write_user_en = 1;
	config._.decounter_id = 0;
	config._.decounted = 0;
	config._.payload_min = 1;
	config._.payload_max = 32;
	config._.bw_current_credit = 0xff;
	config._.bw_max_credit     = 0xff;
	config._.bw_fast_delay     = 0x00;
	config._.bw_slow_delay     = 0x00;

	header._.tag = data.min_rx;
	header._.valid = 1;

	ret = mppa_noc_dnoc_tx_configure(nocIf, nocTx, header, config);
	if (ret != MPPA_NOC_RET_SUCCESS) {
		fprintf(stderr, "[ETH] Error: Failed to configure Tx\n");
		goto open_err;
	}

	status[eth_if].cluster[remoteClus].txId = nocTx;
	status[eth_if].cluster[remoteClus].min_rx = data.min_rx;
	status[eth_if].cluster[remoteClus].max_rx = data.max_rx;

	context =  &mppa_dnoc[nocIf]->tx_chan_route[nocTx].
		min_max_task_id[ETH_DEFAULT_CTX];

	context->_.current_task_id = data.min_rx;
	context->_.min_task_id = data.min_rx;
	context->_.max_task_id = data.max_rx;
	context->_.min_max_task_id_en = 1;

	/* Configure dispatcher so that the defaulat "MATCH ALL" also
	 * sends packet to our cluster */
	mppabeth_lb_cfg_table_rr_dispatch_channel((void *)&(mppa_ethernet[0]->lb),
						  ETH_MATCHALL_TABLE_ID,
						  eth_if, nocIf - 4, nocTx,
						  (1 << ETH_DEFAULT_CTX));

	/* Now deal with Tx */
	unsigned rx_port;
	ret = mppa_noc_dnoc_rx_alloc_auto(nocIf, &rx_port, MPPA_NOC_NON_BLOCKING);
	if(ret) {
		fprintf(stderr, "[ETH] Error: Failed to find an available Rx on DMA %d\n", nocIf);
		goto open_err;
	}

	mppa_dnoc_queue_event_it_target_t it_targets = {
		.reg = 0
	};
	mppa_noc_dnoc_rx_configuration_t conf = {
		.buffer_base = (unsigned long)(void*)
		&mppa_ethernet[0]->tx.fifo_if[0].lane[eth_if].eth_fifo[remoteClus].push_data,
		.buffer_size = 8,
		.current_offset = 0,
		.event_counter = 0,
		.item_counter = 1,
		.item_reload = 1,
		.reload_mode = MPPA_NOC_RX_RELOAD_MODE_INCR_DATA_NOTIF,
		.activation = MPPA_NOC_ACTIVATED | MPPA_NOC_FIFO_MODE,
		.counter_id = 0,
		.event_it_targets = &it_targets,
	};
	ret = mppa_noc_dnoc_rx_configure(nocIf, rx_port, conf);
	if(ret) {
		fprintf(stderr, "[ETH] Error: Failed to configure Rx\n");
		goto open_err;
	}

	status[eth_if].cluster[remoteClus].rx_tag = rx_port;

	ack.open.eth_tx_if = externalAddress;
	ack.open.eth_tx_tag = rx_port;

	return ack;

 open_err:
	mppa_noc_dnoc_tx_free(nocIf, nocTx);
	status[eth_if].cluster[remoteClus].txId = -1;
 err:
	ack.status = 1;
	return ack;
}

odp_rpc_cmd_ack_t  eth_close(unsigned remoteClus, odp_rpc_t *msg)
{
	odp_rpc_cmd_ack_t ack = { .status = 0 };
	odp_rpc_cmd_clos_t data = { .inl_data = msg->inl_data };
	const uint32_t nocIf = get_eth_dma_id(remoteClus);
	const int nocTx = status[data.ifId].cluster[remoteClus].txId;
	const unsigned int eth_if = data.ifId % 4; /* 4 is actually 0 in 40G mode */

	if(nocTx < 0) {
		ack.status = -1;
		return ack;
	}

	/* Deconfigure DMA/Tx in the RR bitmask */
	mppabeth_lb_cfg_table_rr_dispatch_channel((void *)&(mppa_ethernet[0]->lb),
						  ETH_MATCHALL_TABLE_ID,
						  eth_if, nocIf - 4, nocTx,
						  (1 << ETH_DEFAULT_CTX));
	/* Close the Tx */
	mppa_noc_dnoc_tx_free(nocIf, nocTx);
	/* Close the Rx */
	mppa_noc_dnoc_tx_free(nocIf, status[eth_if].cluster[remoteClus].rx_tag);
	status[eth_if].cluster[remoteClus].txId = -1;

	return ack;
}
void eth_init(void)
{
	/* "MATCH_ALL" Rule */
	mppabeth_lb_cfg_rule((void *)&(mppa_ethernet[0]->lb),
			     ETH_MATCHALL_TABLE_ID, ETH_MATCHALL_RULE_ID,
			     /* offset */ 0, /* Cmp Mask */0,
			     /* Espected Value */ 0, /* Hash. Unused */0);

	for (int ifId = 0; ifId < N_ETH_LANE; ++ifId) {
		_eth_status_init(&status[ifId]);

		mppabeth_lb_cfg_header_mode((void *)&(mppa_ethernet[0]->lb),
					    ifId, MPPABETHLB_ADD_HEADER);
		mppabeth_lb_cfg_extract_table_mode((void *)&(mppa_ethernet[0]->lb),
						   0, 0, MPPABETHLB_DISPATCH_POLICY_RR);
		mppabeth_lb_cfg_table_rr_dispatch_trigger((void *)&(mppa_ethernet[0]->lb),
							  ETH_MATCHALL_TABLE_ID,
							  ifId, 1);

	}
}

void eth_send_pkts(void){
	static int data_counter = 0;
	odp_rpc_t buf;
	memset(&buf, 0, sizeof(buf));
	buf.pkt_type = ODP_RPC_CMD_BAS_PING;

	for (int ethIf = 0; ethIf < N_ETH_LANE; ++ethIf) {
		for (int clus = 0; clus < BSP_NB_CLUSTER_MAX; ++clus) {
			const int nocTx = status[ethIf].cluster[clus].txId;

			if(nocTx < 0)
				continue;

			const int nocIf = get_eth_dma_id(clus);
			if(nocIf < 0)
				continue;
			const int minRx = status[ethIf].cluster[clus].min_rx;
			const int maxRx = status[ethIf].cluster[clus].max_rx;

			for( int rx = minRx; rx <= maxRx; ++rx) {
				mppa_dnoc[nocIf]->tx_channels[nocTx].
					header._.tag = rx;
				mppa_noc_dnoc_tx_send_data_eot(nocIf, nocTx,
							       sizeof(buf), &buf);
				buf.inl_data.data[0] = data_counter++;
			}
		}
	}
}
