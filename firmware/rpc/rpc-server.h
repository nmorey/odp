#ifndef RPC_SERVER__H
#define RPC_SERVER__H

typedef void (*odp_rpc_handler_t)(unsigned remoteClus, odp_rpc_t * msg, uint8_t * payload);

int odp_rpc_server_start(odp_rpc_handler_t handler);
int odp_rpc_server_poll_msg(odp_rpc_t **msg, uint8_t **payload);
int odp_rpc_server_ack(odp_rpc_t * msg, odp_rpc_cmd_ack_t ack);

#endif /* RPC_SERVER__H */
