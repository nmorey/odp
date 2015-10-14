#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <mppa_power.h>
#include <mppa_routing.h>
#include <mppa_noc.h>
#include <mppa_bsp.h>
#include <mppa/osconfig.h>

#include "odp_rpc_internal.h"
#include "rpc-server.h"

#include "mppa_pcie_noc.h"
#include "boot.h"

#define MAX_ARGS                       10
#define MAX_CLUS_NAME                  256

#define PCIE_ETH_INTERFACE_COUNT	1

int main (int argc, char *argv[])
{
	int ret;

	if (argc < 2) {
		printf("Missing arguments\n");
		exit(1);
	}


	mppa_pcie_eth_noc_init();

	ret = odp_rpc_server_start();
	if (ret) {
		fprintf(stderr, "[RPC] Error: Failed to start server\n");
		exit(EXIT_FAILURE);
	}
	ret = mppa_pcie_eth_init(PCIE_ETH_INTERFACE_COUNT);
	if (ret != 0) {
		fprintf(stderr, "Failed to initialize PCIe eth interface\n");
		exit(1);
	}

	boot_clusters(argc, argv);

	while (1) {
		odp_rpc_t *msg;

		if (odp_rpc_server_handle(&msg) < 0) {
			fprintf(stderr, "[RPC] Error: Unhandled message\n");
			exit(1);
		}

		mppa_pcie_eth_handler();
	}

	return 0;
}
