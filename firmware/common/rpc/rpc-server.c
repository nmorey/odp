#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include <HAL/hal/hal.h>

#include <odp_rpc_internal.h>
#include <odp/plat/atomic_types.h>

#include <mppa_bsp.h>
#include <mppa_routing.h>
#include <mppa_noc.h>
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
  #if defined(K1B_EXPLORER)
	(void)ifId;
	remoteClus = (tag - RPC_BASE_RX);
  #else
	int locIfId = ifId;
    #if defined(__k1b__)
	locIfId = locIfId - 4;
    #endif
	remoteClus = 4 * locIfId + (tag - RPC_BASE_RX);
  #endif
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
	(void)handler;
	g_clus_priv[clus_id].recv_buf = malloc(RPC_PKT_SIZE);
	if (!g_clus_priv[clus_id].recv_buf)
		return 1;

	int ifId;
	int rxId;

	ifId = get_dma_id(clus_id);
	rxId = get_tag_id(clus_id);

	/* DNoC */
	ret = mppa_noc_dnoc_rx_alloc(ifId, rxId);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	mppa_noc_dnoc_rx_configuration_t conf = {
		.buffer_base = (uintptr_t)g_clus_priv[clus_id].recv_buf,
		.buffer_size = RPC_PKT_SIZE,
		.current_offset = 0,
		.item_counter = 0,
		.item_reload = 0,
		.reload_mode = MPPA_NOC_RX_RELOAD_MODE_INCR_DATA_NOTIF,
		.activation = MPPA_NOC_ACTIVATED,
		.counter_id = 0
	};

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
			int ev_counter = mppa_noc_dnoc_rx_lac_event_counter(interface_id, rx_id);
			assert(ev_counter > 0);
			return rx_id;
		}
	}
	return -1;
}
int odp_rpc_server_poll_msg(odp_rpc_t **msg, uint8_t **payload)
{
	const int base_if = (__bsp_flavour == BSP_ETH_530) ? 4 : 0;
	int idx;
	
	for (idx = 0; idx < BSP_NB_DMA_IO; ++idx) {
		int if_id = idx + base_if;
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

	unsigned interface = get_dma_id(msg->dma_id);

	return odp_rpc_send_msg(interface, msg->dma_id, msg->dnoc_tag, msg, NULL);
}
