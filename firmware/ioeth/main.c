#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include <HAL/hal/hal.h>

#include "odp_rpc_internal.h"
#include "rpc-server.h"
#include "eth.h"

int main()
{

	int ret;

	eth_init();

	ret = odp_rpc_server_start();
	if (ret) {
		fprintf(stderr, "Failed to start server\n");
		exit(EXIT_FAILURE);
	}


	while (1) {
		odp_rpc_t *msg;

		if (odp_rpc_server_handle(&msg) < 0) {
			fprintf(stderr, "[RPC] Error: Unhandled message\n");
			exit(1);
		}
	}
	return 0;
}
