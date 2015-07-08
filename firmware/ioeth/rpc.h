#define RPC_BASE_RX 10
#define RPC_MAX_PAYLOAD 1024 /* max payload in bytes */
typedef struct {
	uint16_t pkt_type;
	uint16_t data_len;       /* Packet is data len * 8B long. data_len < RPC_MAX_PAYLOAD / 8 */
	uint8_t  dma_id;
	uint8_t  cnoc_id;
	uint64_t inline_data[4];
} odp_rpc_t;

void rpcHandle(unsigned remoteClus, odp_rpc_t * msg);
