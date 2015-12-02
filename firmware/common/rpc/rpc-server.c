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

odp_rpc_handler_t __rpc_handlers[MAX_RPC_HANDLERS];
int __n_rpc_handlers;
static uint64_t __rpc_ev_masks[BSP_NB_DMA_IO_MAX][4];

static struct {
	void    *recv_buf;
} g_clus_priv[BSP_NB_CLUSTER_MAX];

static inline int rxToMsg(unsigned ifId, unsigned tag,
			   odp_rpc_t **msg, uint8_t **payload)
{
	int remoteClus;
#if defined(K1B_EXPLORER)
	(void)ifId;
	remoteClus = (tag - RPC_BASE_RX);
#elif defined(__ioddr__)
	remoteClus = ifId + 4 * (tag - RPC_BASE_RX);
#elif defined(__ioeth__)
	int locIfId = ifId;
  #if defined(__k1b__)
	locIfId = locIfId - 4;
  #endif
	remoteClus = 4 * locIfId + (tag - RPC_BASE_RX);
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
#ifdef VERBOSE
	odp_rpc_print_msg(cmd);
#endif

	return remoteClus;
}

static int cluster_init_dnoc_rx(int clus_id)
{
	mppa_noc_ret_t ret;
	g_clus_priv[clus_id].recv_buf = malloc(RPC_PKT_SIZE);
	if (!g_clus_priv[clus_id].recv_buf)
		return 1;

	int ifId;
	int rxId;

	ifId = get_rpc_dma_id(clus_id);
	rxId = get_rpc_tag_id(clus_id);
	__rpc_ev_masks[ifId][rxId / 64] |= 1ULL << (rxId % 64);

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

	return 0;
}

int odp_rpc_server_start(void)
{
	int i;

	for (i = 0; i < BSP_NB_CLUSTER_MAX; ++i) {
		int ret = cluster_init_dnoc_rx(i);
		if (ret)
			return ret;
	}

#ifdef VERBOSE
	printf("[RPC] Server started...\n");
#endif
	g_rpc_init = 1;

	return 0;
}
static int get_if_rx_id(unsigned interface_id)
{
	int i;

	mppa_noc_dnoc_rx_bitmask_t bitmask = mppa_noc_dnoc_rx_get_events_bitmask(interface_id);
	for (i = 0; i < 3; ++i) {
		bitmask.bitmask[i] &= __rpc_ev_masks[interface_id][i];
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

	unsigned interface = get_rpc_dma_id(msg->dma_id);

	return odp_rpc_send_msg(interface, msg->dma_id, msg->dnoc_tag, msg, NULL);
}

int odp_rpc_server_handle(odp_rpc_t ** unhandled_msg)
{
	int remoteClus;
	odp_rpc_t *msg;
	uint8_t *payload;
	remoteClus = odp_rpc_server_poll_msg(&msg, &payload);
	if(remoteClus >= 0) {
		for (int i = 0; i < __n_rpc_handlers; ++i) {
			if (!__rpc_handlers[i](remoteClus, msg, payload))
				return 1;
		}
		*unhandled_msg = msg;
		return -1;
	}
	return 0;
}

static int bas_rpc_handler(unsigned remoteClus, odp_rpc_t *msg, uint8_t *payload)
{
	odp_rpc_cmd_ack_t ack = ODP_RPC_CMD_ACK_INITIALIZER;

	(void)remoteClus;
	(void)payload;
	switch (msg->pkt_type){
	case ODP_RPC_CMD_BAS_PING:
		ack.status = 0;
		break;
	default:
		return -1;
	}
	odp_rpc_server_ack(msg, ack);
	return 0;
}

void  __attribute__ ((constructor)) __bas_rpc_constructor()
{
	if(__n_rpc_handlers < MAX_RPC_HANDLERS) {
		__rpc_handlers[__n_rpc_handlers++] = bas_rpc_handler;
	} else {
		fprintf(stderr, "Failed to register BAS RPC handlers\n");
		exit(EXIT_FAILURE);
	}
}
