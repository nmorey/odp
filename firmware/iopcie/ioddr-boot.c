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

#define MAX_ARGS                       10
#define MAX_CLUS_NAME                  256

struct clus_bin_boot {
	int clus_id;
	char *clus_bin;
	const char *clus_argv[MAX_ARGS];
	int clus_argc;
	odp_rpc_t msg;
};

struct clus_bin_boot clus_bin_boots[BSP_NB_CLUSTER_MAX] = {{0}};

static void io_wait_cluster_sync(int clus_count)
{
	odp_rpc_t *tmp_msg;
	odp_rpc_cmd_ack_t ack = {.status = 0};
	int booted_clus = 0, clus;

	while (1) {
		clus = odp_rpc_server_poll_msg(&tmp_msg, NULL);
		if(clus >= 0) {
			if (tmp_msg->pkt_type != ODP_RPC_CMD_BAS_SYNC) {
				printf("Receive invalid rpc pkt type\n");
				exit(1);
			}
			clus_bin_boots[clus].msg = *tmp_msg;
			booted_clus++;
			if (booted_clus == clus_count) {
				break;
			}
		}

	}

	for (clus = 0; clus < clus_count; clus++) {
		odp_rpc_server_ack(&clus_bin_boots[clus].msg, ack);		
	}
}

int main (int argc, char *argv[])
{
	int i, clus_count = 0, clus_status, opt;
	int ret;
	mppa_power_pid_t clus_pid[BSP_NB_CLUSTER_MAX];
	uint64_t clus_mask = 0;

	if (argc < 2) {
		printf("Missing arguments\n");
		exit(1);
	}


	while ((opt = getopt(argc, argv, "c:a:i:")) != -1) {
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
		case 'i':
			clus_bin_boots[clus_count - 1].clus_id = atoi(optarg);
			break;
		default: /* '?' */
			fprintf(stderr, "Wrong arguments\n");
			exit(EXIT_FAILURE);
		}
	}

	mppa_power_init();

	ret = odp_rpc_server_start(NULL);
	if (ret) {
		fprintf(stderr, "[RPC] Error: Failed to start server\n");
		exit(EXIT_FAILURE);
	}

	printf("Spawning %d clusters\n", clus_count);

	for (i = 0; i < clus_count; i++)
		clus_mask |= (1 << clus_bin_boots[i].clus_id);



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


	io_wait_cluster_sync(clus_count);

	/* Poll rpc and eth */

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
