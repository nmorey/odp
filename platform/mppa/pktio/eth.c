/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */
#include <odp_packet_io_internal.h>
#include "HAL/hal/hal.h"
#include <odp/errno.h>
#include <errno.h>
#include "odp_rpc_internal.h"


#define MAX_ETH_SLOTS 2
#define MAX_ETH_PORTS 4
#define N_RX_PER_PORT 10

#include <mppa_noc.h>

static int eth_init(void)
{
	return 0;
}


static int eth_open(odp_pktio_t id ODP_UNUSED, pktio_entry_t *pktio_entry,
		    const char *devname, odp_pool_t pool)
{
	if (devname[0] != 'p')
		return -1;

	int slot_id = devname[1] - '0';
	int port_id = 4;
	if (slot_id < 0 || slot_id >= MAX_ETH_SLOTS)
		return -1;

	if (devname[2] != 0) {
		if(devname[2] != 'p')
			return -1;
		port_id = devname[3] - '0';

		if (port_id < 0 || port_id >= MAX_ETH_PORTS)
			return -1;

		if(devname[4] != 0)
			return -1;
	}

	pktio_entry->s.pkt_eth.slot_id = slot_id;
	pktio_entry->s.pkt_eth.port_id = port_id;
	pktio_entry->s.pkt_eth.pool = pool;

	int first_rx = -1;
	int n_rx;
	for (n_rx = 0; n_rx < N_RX_PER_PORT; ++n_rx) {
		mppa_noc_ret_t ret;
		unsigned rx_port;

		ret = mppa_noc_dnoc_rx_alloc_auto(0, &rx_port, MPPA_NOC_BLOCKING);
		if(ret != MPPA_NOC_RET_SUCCESS)
			break;

		if (first_rx >= 0 && (unsigned)(first_rx + n_rx) != rx_port) {
			/* Non contiguous port... Fail */
			mppa_noc_dnoc_rx_free(0, rx_port);
			break;
		}
		if(first_rx < 0)
			first_rx = rx_port;
	}
	if (n_rx < N_RX_PER_PORT) {
		/* Something went wrong. Free the ports */

		/* Last one was a failure or
		 * non contiguoues (thus freed already) */
		n_rx --;

		for ( ;n_rx >= 0; --n_rx)
			mppa_noc_dnoc_rx_free(0, first_rx + n_rx);
	}
	odp_rpc_cmd_open_t open_cmd = {
		{
			.ifId = port_id,
			.min_rx = first_rx,
			.max_rx = first_rx + n_rx - 1
		}
	};
	unsigned cluster_id = __k1_get_cluster_id();
	odp_rpc_t cmd = {
		.pkt_type = ODP_RPC_CMD_ETH_OPEN,
		.data_len = 0,
		.flags = 0,
		.inl_data = open_cmd.inl_data
	};
	odp_rpc_do_query(odp_rpc_get_ioeth_dma_id(slot_id, cluster_id),
			 odp_rpc_get_ioeth_tag_id(slot_id, cluster_id),
			 &cmd, NULL);
	odp_rpc_wait_ack(&cmd, NULL);
	odp_rpc_cmd_ack_t ack_cmd = { .inl_data = cmd.inl_data};
	return ack_cmd.status;
}

static int eth_close(pktio_entry_t * const pktio_entry)
{

	int slot_id = pktio_entry->s.pkt_eth.slot_id;
	int port_id = pktio_entry->s.pkt_eth.port_id;

	odp_rpc_cmd_clos_t close_cmd = {
		{
			.ifId = pktio_entry->s.pkt_eth.port_id = port_id

		}
	};
	unsigned cluster_id = __k1_get_cluster_id();
	odp_rpc_t cmd = {
		.pkt_type = ODP_RPC_CMD_ETH_CLOS,
		.data_len = 0,
		.flags = 0,
		.inl_data = close_cmd.inl_data
	};
	odp_rpc_do_query(odp_rpc_get_ioeth_dma_id(slot_id, cluster_id),
			 odp_rpc_get_ioeth_tag_id(slot_id, cluster_id),
			 &cmd, NULL);

	odp_rpc_wait_ack(&cmd, NULL);
	odp_rpc_cmd_ack_t ack_cmd = { .inl_data = cmd.inl_data};
	return ack_cmd.status;
}

static int eth_mac_addr_get(pktio_entry_t *pktio_entry ODP_UNUSED,
			    void *mac_addr ODP_UNUSED)
{
	/* FIXME */
	return -1;
}

static int eth_recv(pktio_entry_t *pktio_entry ODP_UNUSED,
		    odp_packet_t pkt_table[] ODP_UNUSED,
		    unsigned len ODP_UNUSED)
{

	return 0;
}

static int eth_send(pktio_entry_t *pktio_entry ODP_UNUSED,
		    odp_packet_t pkt_table[] ODP_UNUSED,
		    unsigned len ODP_UNUSED)
{
	return 0;
}

static int eth_promisc_mode_set(pktio_entry_t *const pktio_entry ODP_UNUSED,
				odp_bool_t enable ODP_UNUSED){
	return -1;
}

static int eth_promisc_mode(pktio_entry_t *const pktio_entry ODP_UNUSED){
	return -1;
}

static int eth_mtu_get(pktio_entry_t *const pktio_entry ODP_UNUSED) {
	return -1;
}
const pktio_if_ops_t eth_pktio_ops = {
	.init = eth_init,
	.open = eth_open,
	.close = eth_close,
	.recv = eth_recv,
	.send = eth_send,
	.mtu_get = eth_mtu_get,
	.promisc_mode_set = eth_promisc_mode_set,
	.promisc_mode_get = eth_promisc_mode,
	.mac_get = eth_mac_addr_get,
};
