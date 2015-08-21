#include <stdio.h>
#include <mppa_power.h>
#include <mppa_routing.h>
#include <mppa_noc.h>
#include <mppa_bsp.h>
#include <mppa/osconfig.h>

#include "odp_rpc_internal.h"
#include "rpc-server.h"


void rpcHandle(unsigned remoteClus, odp_rpc_t *msg, uint8_t *payload)
{

	(void)remoteClus;
	(void)payload;
	odp_rpc_cmd_ack_t ack = {.status = -1 };
	odp_rpc_server_ack(msg, ack);
}

/**
 * Cluster tag to recevie IO sync
 */
#define CNOC_CLUS_SYNC_RX_ID		16
#define CNOC_IO_TX_ID			0

#define NOC_IO_IFACE_ID			0

#define MAX_ARGS			10
#define MAX_CLUS_NAME			256

static int io_init_cnoc_rx(uint64_t clus_mask)
{
#ifdef __k1b__
	mppa_cnoc_mailbox_notif_t notif = {0};
#endif
	mppa_noc_ret_t ret;
	mppa_noc_cnoc_rx_configuration_t conf = {0};
	int rx_id = CNOC_CLUS_SYNC_RX_ID;

	conf.mode = MPPA_NOC_CNOC_RX_BARRIER;
	conf.init_value = clus_mask;

	/* CNoC */
	ret = mppa_noc_cnoc_rx_alloc(NOC_IO_IFACE_ID, rx_id);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	ret = mppa_noc_cnoc_rx_configure(NOC_IO_IFACE_ID, rx_id, conf
#ifdef __k1b__
		, &notif
#endif
	);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	return 0;
}

static int io_init_cnoc_tx()
{
	mppa_noc_ret_t ret;

	/* CnoC */
	ret = mppa_noc_cnoc_tx_alloc(NOC_IO_IFACE_ID, CNOC_IO_TX_ID);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	return 0;
}

static int io_wait_cluster_sync(int cluster_count)
{
	int clus;
	mppa_noc_ret_t nret;
	mppa_routing_ret_t rret;
	mppa_cnoc_config_t config = {0};
	mppa_cnoc_header_t header = {0};

	printf("Waiting for clusters to have booted\n");

	mppa_noc_cnoc_clear_rx_event(NOC_IO_IFACE_ID, CNOC_CLUS_SYNC_RX_ID);

#ifdef __k1b__
	while(mppa_noc_cnoc_rx_get_value(NOC_IO_IFACE_ID, CNOC_CLUS_SYNC_RX_ID) != 0);
#else
	mppa_noc_wait_clear_event(NOC_IO_IFACE_ID, MPPA_NOC_INTERRUPT_LINE_CNOC_RX, CNOC_CLUS_SYNC_RX_ID);
#endif

	printf("Got cluster sync, sending ack\n");

	for(clus = 0; clus < cluster_count; clus++) {
		rret = mppa_routing_get_cnoc_unicast_route(__k1_get_cluster_id(),
							   clus,
							   &config, &header);
		if (rret != MPPA_ROUTING_RET_SUCCESS)
			return 1;

		header._.tag = CNOC_CLUS_SYNC_RX_ID;

		nret = mppa_noc_cnoc_tx_configure(NOC_IO_IFACE_ID, CNOC_IO_TX_ID,
						  config, header);
		if (nret != MPPA_NOC_RET_SUCCESS)
			return 1;

		mppa_noc_cnoc_tx_push_eot(NOC_IO_IFACE_ID, CNOC_IO_TX_ID, 0x1);
	}

	return 0;
}

struct clus_bin_boot {
	int clus_id;
	char *clus_bin;
	const char *clus_argv[MAX_ARGS];
	int clus_argc;
};

struct clus_bin_boot clus_bin_boots[BSP_NB_CLUSTER_MAX] = {{0}};

int main (int argc, char *argv[])
{
	int i, clus_count = 0, clus_status, opt;
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
	odp_rpc_server_start(rpcHandle);

	printf("Spawning %d clusters\n", clus_count);

	for (i = 0; i < clus_count; i++)
		clus_mask |= (1 << clus_bin_boots[i].clus_id);

	io_init_cnoc_rx(~clus_mask);

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


	io_init_cnoc_tx();
	io_wait_cluster_sync(clus_count);

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
