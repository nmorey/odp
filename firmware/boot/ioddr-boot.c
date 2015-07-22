#include <mppa_power.h>
#include <mppa_routing.h>
#include <mppa_noc.h>
#include <mppa_bsp.h>
#include <mppa/osconfig.h>

#include <stdio.h>

/**
 * Cluster tag to recevie IO sync
 */
#define CNOC_CLUS_SYNC_RX_ID		16
#define CNOC_IO_TX_ID			0

#define NOC_IO_IFACE_ID			0


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

	while ((volatile bool) mppa_noc_has_pending_event(NOC_IO_IFACE_ID, MPPA_NOC_INTERRUPT_LINE_CNOC_RX) !=
			true);

	mppa_noc_cnoc_clear_rx_event(NOC_IO_IFACE_ID, CNOC_CLUS_SYNC_RX_ID);

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

int main (int argc, char *argv[])
{
	int i, clus_count, clus_status;
	char clus_id[4];
	mppa_power_pid_t clus_pid[BSP_NB_CLUSTER_MAX];
	const char *clus_argv[3];
	uint64_t clus_mask = 0;

	if (argc < 2) {
		printf("Missing arguments\n");
		exit(1);
	}

	mppa_power_init();

	clus_count = argc - 1;
	printf("Spawning %d clusters\n", clus_count);

	for (i = 0; i < clus_count; i++)
		clus_mask |= (1 << i);

	io_init_cnoc_rx(~clus_mask);

	for (i = 0; i < clus_count; i++) {
		sprintf(clus_id, "%d", i);
		clus_argv[0] = argv[i + 1];
		clus_argv[1] = clus_id;
		clus_argv[2] = NULL;

		printf("Spawning %s on cluster %d\n", clus_argv[0], i);
		clus_pid[i] = mppa_power_base_spawn(i, clus_argv[0], clus_argv, NULL, MPPA_POWER_SHUFFLING_DISABLED);
	}


	io_init_cnoc_tx();
	io_wait_cluster_sync(clus_count);

	for (i = 0; i < clus_count; i++) {
		if (mppa_power_base_waitpid(clus_pid[i], &clus_status, 0) != clus_pid[i]) {
			printf("Failed to wait cluster %d\n", i);
		}
	}
}
