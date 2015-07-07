#define RPC_BASE_RX 10
#define RPC_MAX_PAYLOAD 1024 /* max payload in bytes */
typedef struct {
	uint16_t pkt_type;
	uint16_t data_len;       /* Packet is data len * 8B long. data_len < RPC_MAX_PAYLOAD / 8 */

	uint64_t inline_data[4];
} odp_rpc_t;
