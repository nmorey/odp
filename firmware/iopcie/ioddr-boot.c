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

#define MAX_ARGS                       10
#define MAX_CLUS_NAME                  256

enum state {
	STATE_BOOT = 0,
	STATE_RUN,
	STATE_STOP,
};


struct clus_bin_boot {
	int clus_id;
	char *clus_bin;
	const char *clus_argv[MAX_ARGS];
	int clus_argc;
	odp_rpc_t msg;
	int sync_status;
};

static unsigned int clus_count = 0;
static enum state current_state = STATE_BOOT;

struct clus_bin_boot clus_bin_boots[BSP_NB_CLUSTER_MAX] = {{0}};

static int io_check_cluster_sync_status()
{
	unsigned int clus;
	odp_rpc_cmd_ack_t ack = {.status = 0 };

	/* Check that all clusters have synced */
	for( clus = 0; clus < clus_count; clus++) {
		if (clus_bin_boots[clus].sync_status == 0)
			return 0;
	}

	/* If so ack them */
	for (clus = 0; clus < clus_count; clus++) {
		clus_bin_boots[clus].sync_status = 0;
		printf("sending ack to %d\n", clus);
		odp_rpc_server_ack(&clus_bin_boots[clus].msg, ack);
	}

	current_state += 1;

	return 1;
}

static odp_rpc_cmd_ack_t rpcHandle(unsigned remoteClus, odp_rpc_t * msg)
{
	odp_rpc_cmd_ack_t ack = ODP_RPC_CMD_ACK_INITIALIZER;

	switch (msg->pkt_type){
	case ODP_RPC_CMD_PCIE_OPEN:
		return mppa_pcie_eth_open(remoteClus, msg);
		break;
	case ODP_RPC_CMD_BAS_SYNC:
		printf("received sync req from clus %d\n", remoteClus);
		clus_bin_boots[remoteClus].msg = *msg;
		clus_bin_boots[remoteClus].sync_status = 1;
		ack.status = 0;
		break;
	case ODP_RPC_CMD_BAS_INVL:
	default:
		fprintf(stderr, "[RPC] Error: Invalid MSG\n");
		exit(EXIT_FAILURE);
	}

	return ack;
}

static void iopcie_rpc_poll()
{
	while (1) {
		int remoteClus;
		odp_rpc_t *msg;

		remoteClus = odp_rpc_server_poll_msg(&msg, NULL);
		if(remoteClus < 0)
			continue;

		odp_rpc_cmd_ack_t ack = rpcHandle(remoteClus, msg);
		/* If the command is a sync one, then wait for every clusters to be synced */
		if (msg->pkt_type == ODP_RPC_CMD_BAS_SYNC) {
			io_check_cluster_sync_status();
			if (current_state == STATE_STOP)
				return;
		} else {
			odp_rpc_server_ack(msg, ack);
		}
	}
}


int main (int argc, char *argv[])
{
	unsigned int i;
	int clus_status, opt;
	int ret;
	mppa_power_pid_t clus_pid[BSP_NB_CLUSTER_MAX];

	if (argc < 2) {
		printf("Missing arguments\n");
		exit(1);
	}


	while ((opt = getopt(argc, argv, "c:a:")) != -1) {
		switch (opt) {
		case 'c':
			clus_bin_boots[clus_count].clus_bin = strdup(optarg);
			clus_bin_boots[clus_count].clus_id = clus_count;
			clus_bin_boots[clus_count].clus_argv[0] = clus_bin_boots[clus_count].clus_bin;
			clus_bin_boots[clus_count].clus_argc = 1;
			clus_count++;
			break;
		case 'a':
			clus_bin_boots[clus_count - 1].clus_argv[clus_bin_boots[clus_count].clus_argc] = strdup(optarg);
			clus_bin_boots[clus_count - 1].clus_argc++;
			break;
		default: /* '?' */
			fprintf(stderr, "Wrong arguments\n");
			exit(EXIT_FAILURE);
		}
	}

	mppa_pcie_eth_noc_init();

	printf("Initializing pcie eth interface\n");
	ret = mppa_pcie_eth_init(16);
	if (ret != 0) {
		printf("Failed to initialize PCIe eth interface\n");
		exit(1);
	}

	ret = odp_rpc_server_start(NULL);
	if (ret) {
		fprintf(stderr, "[RPC] Error: Failed to start server\n");
		exit(EXIT_FAILURE);
	}

	printf("Spawning %d clusters\n", clus_count);

	for (i = 0; i < clus_count; i++) {
		clus_bin_boots[i].clus_argv[clus_bin_boots[i].clus_argc] = NULL;
		printf("Spawning %s on cluster %d with %d args\n", clus_bin_boots[i].clus_argv[0], clus_bin_boots[i].clus_id, clus_bin_boots[i].clus_argc);
		clus_pid[i] = mppa_power_base_spawn(clus_bin_boots[i].clus_id,
							clus_bin_boots[i].clus_argv[0],
							clus_bin_boots[i].clus_argv,
							NULL, MPPA_POWER_SHUFFLING_DISABLED);
		if (clus_pid[i] < 0) {
			printf("Failed to spawn cluster %d\n", i);
			return 1;
		}
	}

	/* Poll rpc and eth */
	iopcie_rpc_poll();

	for (i = 0; i < clus_count; i++) {
		if (mppa_power_base_waitpid(clus_pid[i], &clus_status, 0) != clus_pid[i]) {
			printf("Failed to wait cluster %d\n", i);
			return 1;
		}
		printf("Cluster %d return status: %d\n", clus_pid[i],  clus_status);
		if (clus_status != 0)
			return 1;
	}

	return 0;
}
