#ifndef RPC_SERVER__H
#define RPC_SERVER__H

typedef void (*odp_rpc_handler_t)(unsigned remoteClus, odp_rpc_t * msg, uint8_t * payload);

int odp_rpc_server_start(odp_rpc_handler_t handler);
int odp_rpc_server_poll_msg(odp_rpc_t **msg, uint8_t **payload);
int odp_rpc_server_ack(odp_rpc_t * msg, odp_rpc_cmd_ack_t ack);


static inline int get_tag_id(unsigned cluster_id)
{
#if defined(__ioddr__)
	return odp_rpc_get_ioddr_tag_id(0, cluster_id);
#elif defined(__ioeth__)
	return odp_rpc_get_ioeth_tag_id(0, cluster_id);
#else
#error "Neither ioddr nor ioeth"
#endif
}

static inline int get_dma_id(unsigned cluster_id)
{
#if defined(__ioddr__)
	return odp_rpc_get_ioddr_dma_id(0, cluster_id) - 128;
#elif defined(__ioeth__)
	int if_id = odp_rpc_get_ioeth_dma_id(0, cluster_id) - 160;
  #if defined(__k1b__)
	/* On K1B, DMA 0-3 belong to IODDR */
	if_id += 4;
  #endif
	return if_id;
#else
#error "Neither ioddr nor ioeth"
#endif
}

#endif /* RPC_SERVER__H */
