#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include <HAL/hal/hal.h>

#include <odp_rpc_internal.h>
#include <stdio.h>
#include <mppa_noc.h>

#include "rpc-server.h"

typedef struct {
	uint8_t opened     : 1;
	uint8_t rx_enabled : 1;
	uint8_t tx_enabled : 1;
	uint8_t min_rx     : 8;
	uint8_t max_rx     : 8;
	uint8_t cnoc_rx    : 8;
	uint16_t rx_size   :16;
} c2c_status_t;

static c2c_status_t c2c_status[BSP_NB_CLUSTER_MAX][BSP_NB_CLUSTER_MAX];

odp_rpc_cmd_ack_t  c2c_open(unsigned src_cluster, odp_rpc_t *msg)
{
	odp_rpc_cmd_ack_t ack = { .status = 0};
	odp_rpc_cmd_c2c_open_t data = { .inl_data = msg->inl_data };
	const unsigned dst_cluster = data.cluster_id;

	if (c2c_status[src_cluster][dst_cluster].opened){
		fprintf(stderr, "[C2C] Error: %d => %d is already opened\n",
			src_cluster, dst_cluster);
		goto err;
	}

	c2c_status[src_cluster][dst_cluster].opened = 1;
	c2c_status[src_cluster][dst_cluster].rx_enabled = data.rx_enabled;
	c2c_status[src_cluster][dst_cluster].tx_enabled = data.tx_enabled;
	c2c_status[src_cluster][dst_cluster].min_rx = data.min_rx;
	c2c_status[src_cluster][dst_cluster].max_rx = data.max_rx;
	c2c_status[src_cluster][dst_cluster].rx_size = data.mtu;
	c2c_status[src_cluster][dst_cluster].cnoc_rx = data.cnoc_rx;
	return ack;

 err:
	ack.status = 1;
	return ack;
}

odp_rpc_cmd_ack_t  c2c_close(unsigned src_cluster, odp_rpc_t *msg)
{
	odp_rpc_cmd_ack_t ack = { .status = 0};
	odp_rpc_cmd_c2c_clos_t data = { .inl_data = msg->inl_data };
	const unsigned dst_cluster = data.cluster_id;

	if (!c2c_status[src_cluster][dst_cluster].opened){
		fprintf(stderr, "[C2C] Error: %d => %d is not open\n",
			src_cluster, dst_cluster);
		goto err;
	}

	memset(&c2c_status[src_cluster][dst_cluster], 0, sizeof(c2c_status_t));
	return ack;

 err:
	ack.status = 1;
	return ack;
}

odp_rpc_cmd_ack_t  c2c_query(unsigned src_cluster, odp_rpc_t *msg)
{
	odp_rpc_cmd_ack_t ack = { .status = 0};
	odp_rpc_cmd_c2c_query_t data = { .inl_data = msg->inl_data };
	const unsigned dst_cluster = data.cluster_id;

	const c2c_status_t * s2d = &c2c_status[src_cluster][dst_cluster];
	const c2c_status_t * d2s = &c2c_status[dst_cluster][src_cluster];

	if (!s2d->opened || !d2s->opened){
		ack.status = 1;
		ack.cmd.c2c_query.closed = 1;
		return ack;
	}

	if (!s2d->tx_enabled || !d2s->rx_enabled) {
		ack.status = 1;
		ack.cmd.c2c_query.eacces = 1;
		return ack;
	}
	ack.cmd.c2c_query.mtu = d2s->rx_size;
	if (s2d->rx_size < ack.cmd.c2c_query.mtu)
		ack.cmd.c2c_query.mtu = s2d->rx_size;

	ack.cmd.c2c_query.min_rx = d2s->min_rx;
	ack.cmd.c2c_query.max_rx = d2s->max_rx;
	ack.cmd.c2c_query.cnoc_rx = d2s->cnoc_rx;
	return ack;
}

static int c2c_rpc_handler(unsigned remoteClus, odp_rpc_t *msg, uint8_t *payload)
{
	odp_rpc_cmd_ack_t ack = ODP_RPC_CMD_ACK_INITIALIZER;

	(void)payload;
	switch (msg->pkt_type){
	case ODP_RPC_CMD_C2C_OPEN:
		ack = c2c_open(remoteClus, msg);
		break;
	case ODP_RPC_CMD_C2C_CLOS:
		ack = c2c_close(remoteClus, msg);
		break;
	case ODP_RPC_CMD_C2C_QUERY:
		ack = c2c_query(remoteClus, msg);
		break;
	default:
		return -1;
	}
	odp_rpc_server_ack(msg, ack);
	return 0;
}

void  __attribute__ ((constructor)) __c2c_rpc_constructor()
{
	if(__n_rpc_handlers < MAX_RPC_HANDLERS) {
		__rpc_handlers[__n_rpc_handlers++] = c2c_rpc_handler;
	} else {
		fprintf(stderr, "Failed to register C2C RPC handlers\n");
		exit(EXIT_FAILURE);
	}
}
