#ifndef __FIRMWARE__C2C__H__
#define __FIRMWARE__C2C__H__

odp_rpc_cmd_ack_t  c2c_open(unsigned src_cluster, odp_rpc_t *msg);
odp_rpc_cmd_ack_t  c2c_close(unsigned src_cluster, odp_rpc_t *msg);

#endif /* __FIRMWARE__C2C__H__ */
