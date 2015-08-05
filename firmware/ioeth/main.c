#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include <HAL/hal/hal.h>

#include "odp_rpc_internal.h"
#include "rpc-server.h"
#include "eth.h"

odp_rpc_cmd_ack_t rpcHandle(unsigned remoteClus, odp_rpc_t * msg)
{

	(void)remoteClus;
	odp_rpc_cmd_ack_t ack = {.status = -1 };
	switch (msg->pkt_type){
	case ODP_RPC_CMD_ETH_OPEN:
		return eth_open(remoteClus, msg);
		break;
	case ODP_RPC_CMD_ETH_CLOS:
		return eth_close(remoteClus, msg);
		break;
	case ODP_RPC_CMD_BAS_PING:
		ack.status = 0;
		return ack;
		break;
	case ODP_RPC_CMD_BAS_INVL:
	default:
		fprintf(stderr, "Invalid MSG\n");
		exit(EXIT_FAILURE);
	}
	return ack;
}

int main()
{

	int ret;

	eth_init();

	ret = odp_rpc_server_start(NULL);
	if (ret)
		exit(EXIT_FAILURE);


	while (1) {
		int remoteClus;
		odp_rpc_t *msg;

		remoteClus = odp_rpc_server_poll_msg(&msg, NULL);
		if(remoteClus >= 0) {
			odp_rpc_cmd_ack_t ack = rpcHandle(remoteClus, msg);
			odp_rpc_server_ack(msg, ack);
		}
	}
	return 0;
}
