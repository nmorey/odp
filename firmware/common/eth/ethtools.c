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

int ethtool_setup_eth2clus(unsigned remoteClus, int eth_if,
			   int nocIf, int externalAddress,
			   int min_rx, int max_rx)
{
	int ret;

	mppa_dnoc_header_t header = { 0 };
	mppa_dnoc_channel_config_t config = { 0 };
	unsigned nocTx;

	ret = mppa_routing_get_dnoc_unicast_route(externalAddress,
						  remoteClus, &config, &header);
	if (ret != MPPA_ROUTING_RET_SUCCESS) {
		fprintf(stderr, "[ETH] Error: Failed to route to cluster %d\n", remoteClus);
		return -1;
	}

	ret = mppa_noc_dnoc_tx_alloc_auto(nocIf, &nocTx, MPPA_NOC_BLOCKING);
	if (ret != MPPA_NOC_RET_SUCCESS) {
		fprintf(stderr, "[ETH] Error: Failed to find an available Tx on DMA %d\n", nocIf);
		return -1;
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

	header._.tag = min_rx;
	header._.valid = 1;
	header._.multicast = 0;

	ret = mppa_noc_dnoc_tx_configure(nocIf, nocTx, header, config);
	if (ret != MPPA_NOC_RET_SUCCESS) {
		fprintf(stderr, "[ETH] Error: Failed to configure Tx\n");
		mppa_noc_dnoc_tx_free(nocIf, nocTx);
		return -1;
	}

	status[eth_if].cluster[remoteClus].nocIf = nocIf;
	status[eth_if].cluster[remoteClus].txId = nocTx;
	status[eth_if].cluster[remoteClus].min_rx = min_rx;
	status[eth_if].cluster[remoteClus].max_rx = max_rx;


	volatile mppa_dnoc_min_max_task_id_t *context =
		&mppa_dnoc[nocIf]->tx_chan_route[nocTx].
		min_max_task_id[ETH_DEFAULT_CTX];

	context->_.current_task_id = min_rx;
	context->_.min_task_id = min_rx;
	context->_.max_task_id = max_rx;
	context->_.min_max_task_id_en = 1;

	/* Configure dispatcher so that the defaulat "MATCH ALL" also
	 * sends packet to our cluster */
	mppabeth_lb_cfg_table_rr_dispatch_channel((void *)&(mppa_ethernet[0]->lb),
						  ETH_MATCHALL_TABLE_ID,
						  eth_if, nocIf - 4, nocTx,
						  (1 << ETH_DEFAULT_CTX));
	return 0;
}


int ethtool_setup_clus2eth(unsigned remoteClus, int eth_if, int nocIf)
{
	int ret;
	unsigned rx_port;

	mppa_dnoc[nocIf]->rx_global.rx_ctrl._.alert_level = -1;
	mppa_dnoc[nocIf]->rx_global.rx_ctrl._.payload_slice = 2;
	ret = mppa_noc_dnoc_rx_alloc_auto(nocIf, &rx_port, MPPA_NOC_NON_BLOCKING);
	if(ret) {
		fprintf(stderr, "[ETH] Error: Failed to find an available Rx on DMA %d\n", nocIf);
		return -1;
	}

	mppa_dnoc_queue_event_it_target_t it_targets = {
		.reg = 0
	};
	int fifo_id = remoteClus;
	if (mac_get_default_mode(eth_if) == MPPA_ETH_MAC_ETHMODE_40G) {
		/* Jumbo frames */
		fifo_id = (remoteClus % 4) * 4;
		mppa_ethernet[0]->tx.fifo_if[nocIf].lane[eth_if].eth_fifo[fifo_id].eth_fifo_ctrl._.jumbo_mode = 1;
	}
	mppa_ethernet[0]->tx.fifo_if[nocIf].lane[eth_if].eth_fifo[fifo_id].eth_fifo_ctrl._.drop_en = 1;
	mppa_noc_dnoc_rx_configuration_t conf = {
		.buffer_base = (unsigned long)(void*)
		&mppa_ethernet[0]->tx.fifo_if[nocIf].lane[eth_if].eth_fifo[fifo_id].push_data,
		.buffer_size = 8,
		.current_offset = 0,
		.event_counter = 0,
		.item_counter = 1,
		.item_reload = 1,
		.reload_mode = MPPA_NOC_RX_RELOAD_MODE_DECR_NOTIF_RELOAD,
		.activation = MPPA_NOC_ACTIVATED | MPPA_NOC_FIFO_MODE,
		.counter_id = 0,
		.event_it_targets = &it_targets,
	};

	ret = mppa_noc_dnoc_rx_configure(nocIf, rx_port, conf);
	if(ret) {
		fprintf(stderr, "[ETH] Error: Failed to configure Rx\n");
		mppa_noc_dnoc_rx_free(nocIf, rx_port);
		return -1;
	}

	status[eth_if].cluster[remoteClus].nocIf = nocIf;
	status[eth_if].cluster[remoteClus].rx_tag = rx_port;
	return 0;
}

int ethtool_init_lane(unsigned eth_if, int loopback)
{
	int ret;

	switch (status[eth_if].initialized) {
	case ETH_LANE_OFF:
		if (loopback) {
#ifdef VERBOSE
			printf("[ETH] Initializing lane %d in loopback\n", eth_if);
#endif
			mppabeth_mac_enable_loopback_bypass((void *)&(mppa_ethernet[0]->mac));
			for (int i = 0; i < N_ETH_LANE; ++i)
				status[eth_if].initialized = ETH_LANE_LOOPBACK;
		} else {
			ret = init_mac(eth_if, -1);
			if(ret) {
				fprintf(stderr,
					"[ETH] Error: Failed to initialize lane %d (%d)\n",
					eth_if, ret);
				return -1;
			}
			status[eth_if].initialized = ETH_LANE_ON;
		}
		break;
	case ETH_LANE_ON:
		if (loopback) {
			fprintf(stderr,
				"[ETH] Error: One lane was enabled. Cannot set lane %d in loopback\n",
				eth_if);
			return -1;
		}
		break;
	case ETH_LANE_LOOPBACK:
		if (!loopback) {
			fprintf(stderr,
				"[ETH] Error: Eth is in loopback. Cannot enable lane %d\n",
				eth_if);
			return -1;
		}
		break;
	default:
		return -1;
	}

	return 0;
}
void ethtool_cleanup_cluster(unsigned remoteClus, unsigned eth_if)
{
	if (status[eth_if].cluster[remoteClus].rx_tag >= 0)
		mppa_noc_dnoc_rx_free(status[eth_if].cluster[remoteClus].nocIf,
				      status[eth_if].cluster[remoteClus].rx_tag);

	if (status[eth_if].cluster[remoteClus].txId >= 0) {

		mppabeth_lb_cfg_table_rr_dispatch_channel((void *)&(mppa_ethernet[0]->lb),
							  ETH_MATCHALL_TABLE_ID, eth_if,
							  status[eth_if].cluster[remoteClus].nocIf,
							  status[eth_if].cluster[remoteClus].txId,
							  0);
		mppa_noc_dnoc_tx_free(status[eth_if].cluster[remoteClus].nocIf,
				      status[eth_if].cluster[remoteClus].txId);

	}
	_eth_cluster_status_init(&(status[eth_if].cluster[remoteClus]));
}

int ethtool_enable_cluster(unsigned remoteClus, unsigned eth_if)
{
	if(status[eth_if].cluster[remoteClus].nocIf < 0 ||
	   status[eth_if].cluster[remoteClus].enabled) {
		return -1;
	}

	status[eth_if].cluster[remoteClus].enabled = 1;

	/* if (!status[eth_if].enabled_refcount) */
	/* 	//FIXME */

	status[eth_if].enabled_refcount++;

	return 0;
}
int ethtool_disable_cluster(unsigned remoteClus, unsigned eth_if)
{
	if(status[eth_if].cluster[remoteClus].nocIf < 0 ||
	   status[eth_if].cluster[remoteClus].enabled == 0) {
		return -1;
	}
	status[eth_if].cluster[remoteClus].enabled = 0;
	status[eth_if].enabled_refcount--;

	/* if (!status[eth_if].enabled_refcount) */
	/* 	//FIXME */

	return 0;
}
