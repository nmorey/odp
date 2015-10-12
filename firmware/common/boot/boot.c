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
#include "boot.h"

#define MAX_ARGS                       10
#define MAX_CLUS_NAME                  256

#define PCIE_ETH_INTERFACE_COUNT	1

enum state {
	STATE_BOOT = 0,
	STATE_RUN,
	STATE_STOP,
};

struct clus_bin_boot {
	int id;
	char *bin;
	const char *argv[MAX_ARGS];
	int argc;
	odp_rpc_t msg;
	int sync_status;
	mppa_power_pid_t pid;
};

static unsigned int clus_count;
static enum state current_state = STATE_BOOT;

static struct clus_bin_boot clus_bin_boots[BSP_NB_CLUSTER_MAX];

static int io_check_cluster_sync_status(void)
{
	unsigned int clus;
	odp_rpc_cmd_ack_t ack = {.status = 0 };

	/* Check that all clusters have synced */
	for (clus = 0; clus < clus_count; clus++) {
		if (clus_bin_boots[clus].sync_status == 0)
			return 0;
	}

	/* If so ack them */
	for (clus = 0; clus < clus_count; clus++) {
		clus_bin_boots[clus].sync_status = 0;
		/* printf("sending ack to %d\n", clus); */
		odp_rpc_server_ack(&clus_bin_boots[clus].msg, ack);
	}

	current_state += 1;

	return 1;
}

static int sync_rpc_handler(unsigned remoteClus, odp_rpc_t *msg,
			    uint8_t *payload)
{
	(void)payload;
	switch (msg->pkt_type) {
	case ODP_RPC_CMD_BAS_SYNC:
		/* printf("received sync req from clus %d\n", remoteClus); */
		clus_bin_boots[remoteClus].msg = *msg;
		clus_bin_boots[remoteClus].sync_status = 1;
		io_check_cluster_sync_status();
		if (current_state == STATE_STOP)
			exit(0);
		return 0;
	default:
		return -1;
	}
}

void  __attribute__ ((constructor)) __sync_rpc_constructor()
{
	if (__n_rpc_handlers < MAX_RPC_HANDLERS) {
		__rpc_handlers[__n_rpc_handlers++] = sync_rpc_handler;
	} else {
		fprintf(stderr, "Failed to register SYNC RPC handlers\n");
		exit(EXIT_FAILURE);
	}
}

int boot_clusters(int argc, char * const argv[])
{
	unsigned int i;
	int opt;

	while ((opt = getopt(argc, argv, "c:a:")) != -1) {
		switch (opt) {
		case 'c':
			{
				struct clus_bin_boot *clus =
					&clus_bin_boots[clus_count];
				clus->bin = strdup(optarg);
				clus->id = clus_count;
				clus->argv[0] = clus->bin;
				clus->argc = 1;
				clus_count++;
			}
			break;
		case 'a':
			{
				struct clus_bin_boot *clus =
					&clus_bin_boots[clus_count - 1];
				clus->argv[clus->argc] = strdup(optarg);
				clus->argc++;
			}
			break;
		default: /* '?' */
			fprintf(stderr, "Wrong arguments for boot\n");
			return -1;
		}
	}

	for (i = 0; i < clus_count; i++) {
		struct clus_bin_boot *clus = &clus_bin_boots[i];
		/* printf("Spawning %s on cluster %d with %d args\n", */
		/*        clus->argv[0], */
		/*        clus->id, */
		/*        clus->argc); */
		clus->argv[clus->argc] = NULL;
		clus->pid =
			mppa_power_base_spawn(clus->id,
					      clus->argv[0],
					      clus->argv,
					      NULL,
					      MPPA_POWER_SHUFFLING_DISABLED);
		if (clus->pid < 0) {
			fprintf(stderr, "Failed to spawn cluster %d\n", i);
			return -1;
		}
	}
	return 0;
}
