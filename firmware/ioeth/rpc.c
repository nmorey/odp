#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include <HAL/hal/hal.h>
#include <mppa_ethernet_shared.h>

#include "rpc.h"

void rpcHandle(unsigned remoteClus, odp_rpc_t * msg)
{
	(void)remoteClus;
	(void)msg;
}
