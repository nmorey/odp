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
#include <odp/plat/atomic_types.h>
#include "rpc-server.h"

#define RPC_PKT_SIZE (sizeof(odp_rpc_t) + RPC_MAX_PAYLOAD)

static struct {
	void    *recv_buf;
} g_clus_priv[BSP_NB_CLUSTER_MAX];

static inline int rxToMsg(unsigned ifId, unsigned tag,
			   odp_rpc_t **msg, uint8_t **payload)
{
	int remoteClus;
#if defined(__ioddr__)
	remoteClus = ifId + 4 * (tag - RPC_BASE_RX);
#elif defined(__ioeth__)
	remoteClus = 4 * ifId + (tag - RPC_BASE_RX);
#else
#error "Neither ioddr nor ioeth"
#endif

	odp_rpc_t *cmd = g_clus_priv[remoteClus].recv_buf;
	*msg = cmd;
	INVALIDATE(cmd);

	if(payload && cmd->data_len > 0) {
		*payload = (uint8_t*)(cmd + 1);
		INVALIDATE_AREA(*payload, cmd->data_len);
	}
	return remoteClus;
}

static void dnoc_callback(unsigned interface_id,
			  mppa_noc_interrupt_line_t line,
			  unsigned resource_id, void *args)
{
	odp_rpc_handler_t handler = (odp_rpc_handler_t)(args);
	odp_rpc_t * msg;
	uint8_t * payload = NULL;
	unsigned remoteClus;

	mppa_noc_dnoc_rx_lac_event_counter(interface_id, resource_id);
	(void)line;

	remoteClus = rxToMsg(interface_id, resource_id, &msg, &payload);
	handler(remoteClus, msg, payload);
}

static int cluster_init_dnoc_rx(int clus_id, odp_rpc_handler_t handler)
{
	mppa_noc_ret_t ret;
	mppa_noc_dnoc_rx_configuration_t conf = {0};
	(void)handler;
	g_clus_priv[clus_id].recv_buf = malloc(RPC_PKT_SIZE);
	if (!g_clus_priv[clus_id].recv_buf)
		return 1;

	int ifId;
	int rxId;

#if defined(__ioddr__)
	ifId = clus_id % 4;
	rxId = RPC_BASE_RX + clus_id / 4;
#elif defined(__ioeth__)
	ifId = clus_id / 4;
	rxId = RPC_BASE_RX + clus_id % 4;
#else
#error "Neither ioddr nor ioeth"
#endif

	/* DNoC */
	ret = mppa_noc_dnoc_rx_alloc(ifId, rxId);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	conf.buffer_base = (uintptr_t)g_clus_priv[clus_id].recv_buf;
	conf.buffer_size = RPC_PKT_SIZE;
	conf.activation = MPPA_NOC_ACTIVATED;
	conf.reload_mode = MPPA_NOC_RX_RELOAD_MODE_INCR_DATA_NOTIF;

	ret = mppa_noc_dnoc_rx_configure(ifId, rxId, conf);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	if (handler)
		mppa_noc_register_interrupt_handler(ifId, MPPA_NOC_INTERRUPT_LINE_DNOC_RX,
						    rxId, dnoc_callback, handler);
	return 0;
}

int odp_rpc_server_start(odp_rpc_handler_t handler)
{
	int i;

	for (i = 0; i < BSP_NB_CLUSTER_MAX; ++i) {
		int ret = cluster_init_dnoc_rx(i, handler);
		if (ret)
			return ret;
	}

	return 0;
}
static int get_if_rx_id(unsigned interface_id)
{
	int i;

	mppa_noc_dnoc_rx_bitmask_t bitmask = mppa_noc_dnoc_rx_get_events_bitmask(interface_id);
	for (i = 0; i < 3; ++i) {
		if (bitmask.bitmask[i]) {
			int rx_id = __k1_ctzdl(bitmask.bitmask[i]) + i * 8 * sizeof(bitmask.bitmask[i]);
			mppa_noc_dnoc_rx_lac_event_counter(interface_id, rx_id);

			return rx_id;
		}
	}
	return -1;
}
int odp_rpc_server_poll_msg(odp_rpc_t **msg, uint8_t **payload)
{
	int if_id;

	for (if_id = 0; if_id < 4; ++if_id) {
		int tag = get_if_rx_id(if_id);
		if(tag < 0)
			continue;

		/* Received a message */
		return rxToMsg(if_id, tag, msg, payload);
	}
	return -1;
}

int odp_rpc_server_ack(odp_rpc_t * msg, odp_rpc_cmd_ack_t ack)
{
	msg->ack = 1;
	msg->data_len = 0;
	msg->inl_data = ack.inl_data;

	unsigned interface;

#if defined(__ioddr__)
	interface = msg->dma_id % 4;
#elif defined(__ioeth__)
	interface = msg->dma_id / 4;
#else
#error "Neither ioddr nor ioeth"
#endif

	return odp_rpc_send_msg(interface, msg->dma_id, msg->dnoc_tag, msg, NULL);
}
