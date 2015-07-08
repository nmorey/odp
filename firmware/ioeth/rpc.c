#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include <HAL/hal/hal.h>
#include <mppa_ethernet_shared.h>

#include "rpc.h"
#include "eth.h"

void rpcHandle(unsigned remoteClus, odp_rpc_t * msg)
{
	(void)remoteClus;
	switch (msg->pkt_type){
	case ODP_RPC_CMD_OPEN:
		eth_open_rx(remoteClus, msg);
		break;
	case ODP_RPC_CMD_CLOS:
		eth_close_rx(remoteClus, msg);
		break;
	case ODP_RPC_CMD_INVL:
		break;
	default:
		fprintf(stderr, "Invalid MSG\n");
		exit(EXIT_FAILURE);
	}
}
