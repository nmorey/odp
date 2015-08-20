#ifndef __FIRMWARE__IOETH__ETH__H__
#define __FIRMWARE__IOETH__ETH__H__

#define ETH_BASE_TX 4
#define ETH_DEFAULT_CTX 0
#define ETH_MATCHALL_TABLE_ID 0
#define ETH_MATCHALL_RULE_ID 0

void eth_init(void);
odp_rpc_cmd_ack_t eth_open(unsigned remoteClus, odp_rpc_t * msg);
odp_rpc_cmd_ack_t eth_close(unsigned remoteClus, odp_rpc_t * msg);
void eth_send_pkts(void);

#endif /* __FIRMWARE__IOETH__ETH__H__ */
