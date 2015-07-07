#include <mppa_power.h>
#include <mppa_routing.h>
#include <mppa_noc.h>
#include <mppa_bsp.h>

#include <stdio.h>

int main (int argc, char *argv[])
{
	int i, clus_count, clus_status;
	char clus_id[4];
	mppa_power_pid_t clus_pid[BSP_NB_CLUSTER_MAX];
	const char *clus_argv[3];

	mppa_power_init();

	clus_count = argc - 1;

	for (i = 0; i < clus_count; i++) {
		sprintf(clus_id, "%d", i);
		clus_argv[0] = argv[i + 1];
		clus_argv[1] = clus_id;
		clus_argv[2] = NULL;

		printf("Spawning %s on cluster %d\n", argv[0], i);
		clus_pid[i] = mppa_power_base_spawn(i, clus_argv[0], clus_argv, NULL, MPPA_POWER_SHUFFLING_DISABLED);
	}

	for (i = 0; i < clus_count; i++) {
		if (mppa_power_base_waitpid(clus_pid[i], &clus_status, 0) != clus_pid[i]) {
			printf("Failed to wait cluster %d\n", i);
		}
	}
}
