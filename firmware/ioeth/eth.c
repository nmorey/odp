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

typedef struct {
	int txId;
	int min_rx;
	int max_rx;
} eth_cluster_status_t;
typedef struct {
	eth_cluster_status_t cluster[BSP_NB_CLUSTER_MAX];
} eth_status_t;

eth_status_t status[4];

static inline void _eth_cluster_status_init(eth_cluster_status_t * cluster)
{
	cluster->txId = -1;
	cluster->min_rx = 0;
	cluster->max_rx = -1;
}

static inline void _eth_status_init(eth_status_t * status)
{
	for (int i = 0; i < BSP_NB_CLUSTER_MAX; ++i)
		_eth_cluster_status_init(&status->cluster[i]);
}

odp_rpc_cmd_ack_t  eth_open_rx(unsigned remoteClus, odp_rpc_t *msg)
{
	odp_rpc_cmd_ack_t ack = { .status = 0 };
	odp_rpc_cmd_open_t data = { .inl_data = msg->inl_data };
	const uint32_t nocIf = get_dma_id(remoteClus);
	volatile mppa_dnoc_min_max_task_id_t *context;
	mppa_dnoc_header_t header = { 0 };
	mppa_dnoc_channel_config_t config = { 0 };
	unsigned nocTx;
	int ret;

	if(status[data.ifId].cluster[remoteClus].txId >= 0)
		goto err;

	/* Configure Tx */
	ret = mppa_routing_get_dnoc_unicast_route(__k1_get_cluster_id() + (nocIf % 4),
						  remoteClus, &config, &header);
	if (ret != MPPA_ROUTING_RET_SUCCESS)
		goto err;

	ret = mppa_noc_dnoc_tx_alloc_auto(nocIf, &nocTx, MPPA_NOC_BLOCKING);
	if (ret != MPPA_NOC_RET_SUCCESS)
		goto err;

#ifdef __k1a__
	config.word = 0;
	config._.bandwidth = mppa_noc_dnoc_get_window_length(local_interface);
#else
	config._.loopback_multicast = 0;
	config._.cfg_pe_en = 1;
	config._.cfg_user_en = 1;
	config._.write_pe_en = 1;
	config._.write_user_en = 1;
	config._.decounter_id = 0;
	config._.decounted = 0;
	config._.payload_min = 0;
	config._.payload_max = 32;
	config._.bw_current_credit = 0xff;
	config._.bw_max_credit     = 0xff;
	config._.bw_fast_delay     = 0x00;
	config._.bw_slow_delay     = 0x00;
#endif

	header._.tag = data.min_rx;
	header._.valid = 1;

	ret = mppa_noc_dnoc_tx_configure(nocIf, nocTx, header, config);
	if (ret != MPPA_NOC_RET_SUCCESS)
		goto open_err;

	status[data.ifId].cluster[remoteClus].txId = nocTx;
	status[data.ifId].cluster[remoteClus].min_rx = data.min_rx;
	status[data.ifId].cluster[remoteClus].max_rx = data.max_rx;

	context =  &mppa_dnoc[nocIf]->tx_chan_route[nocTx].
		min_max_task_id[ETH_DEFAULT_CTX];

	context->_.current_task_id = data.min_rx;
	context->_.min_task_id = data.min_rx;
	context->_.max_task_id = data.max_rx;
	context->_.min_max_task_id_en = 1;

	/* Configure dispatcher so that the defaulat "MATCH ALL" also
	 * sends packet to our cluster */
	mppabeth_lb_cfg_table_rr_dispatch_trigger((void *)&(mppa_ethernet[0]->lb),
						  ETH_MATCHALL_TABLE_ID,
						  data.ifId, 1);
	mppabeth_lb_cfg_table_rr_dispatch_channel((void *)&(mppa_ethernet[0]->lb),
						  ETH_MATCHALL_TABLE_ID,
						  data.ifId, nocIf, nocTx,
						  (1 << ETH_DEFAULT_CTX));
	return ack;

 open_err:
	mppa_noc_dnoc_tx_free(nocIf, nocTx);
 err:
	ack.status = 1;
	return ack;
}

odp_rpc_cmd_ack_t  eth_close_rx(unsigned remoteClus, odp_rpc_t *msg)
{
	odp_rpc_cmd_ack_t ack = { .status = 0 };
	odp_rpc_cmd_clos_t data = { .inl_data = msg->inl_data };
	const uint32_t nocIf = get_dma_id(remoteClus);
	const int nocTx = status[data.ifId].cluster[remoteClus].txId;

	if(nocTx < 0) {
		ack.status = -1;
		return ack;
	}

	/* Deconfigure DMA/Tx in the RR bitmask */
	mppabeth_lb_cfg_table_rr_dispatch_channel((void *)&(mppa_ethernet[0]->lb),
						  ETH_MATCHALL_TABLE_ID,
						  data.ifId, nocIf, nocTx,
						  (1 << ETH_DEFAULT_CTX));
	/* Close the Tx */
	mppa_noc_dnoc_tx_free(nocIf, nocTx);
	return ack;
}
void eth_init(void)
{
	init_mac();

	/* "MATCH_ALL" Rule */
	mppabeth_lb_cfg_rule((void *)&(mppa_ethernet[0]->lb),
			     ETH_MATCHALL_TABLE_ID, ETH_MATCHALL_RULE_ID,
			     /* offset */ 0, /* Cmp Mask */0,
			     /* Espected Value */ 0, /* Hash. Unused */0);

	for (int ifId = 0; ifId < 4; ++ifId) {
		_eth_status_init(&status[ifId]);

		mppabeth_lb_cfg_header_mode((void *)&(mppa_ethernet[0]->lb),
					    ifId, MPPABETHLB_ADD_HEADER);
		mppabeth_lb_cfg_footer_mode((void *)&(mppa_ethernet[0]->lb),
					    ifId, MPPABETHLB_ADD_FOOTER);
		mppabeth_lb_cfg_extract_table_mode((void *)&(mppa_ethernet[0]->lb),
						   0, 0, MPPABETHLB_DISPATCH_POLICY_RR);
	}
}

void eth_send_pkts(void){
	static int data_counter = 0;
	odp_rpc_t buf;
	memset(&buf, 0, sizeof(buf));
	buf.pkt_type = ODP_RPC_CMD_BAS_PING;

	for (int ethIf = 0; ethIf < 4; ++ethIf) {
		for (int clus = 0; clus < BSP_NB_CLUSTER_MAX; ++clus) {
			const int nocTx = status[ethIf].cluster[clus].txId;

			if(nocTx < 0)
				continue;

			const int nocIf = get_dma_id(clus);
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
