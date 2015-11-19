#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include <HAL/hal/hal.h>
#include <unistd.h>

#include "odp_rpc_internal.h"
#include "rpc-server.h"
#include "boot.h"

int main()
{

	int ret;

	ret = odp_rpc_server_start();
	if (ret) {
		fprintf(stderr, "[RPC] Error: Failed to start server\n");
		exit(EXIT_FAILURE);
	}

	if ( __k1_get_cluster_id() == 128 ) {
		boot_set_nb_clusters(16);
		printf("Spawning clusters\n");
		{
			static char const * _argv[] = {
				"odp_ipsec.kelf",
				"-i", "e0:loop,e1:loop",
				"-r", "192.168.111.2/32:e0:00.07.43.30.4a.70",
				"-r", "192.168.222.2/32:e1:00.07.43.30.4a.78",
				"-p", "192.168.111.0/24:192.168.222.0/24:out:both",
				"-e", "192.168.111.2:192.168.222.2:aes:201:656c8523255ccc23a66c1917aa0cf309",
				"-a", "192.168.111.2:192.168.222.2:sha256:200:a731649644c5dee92cbd9c2e7e188ee68979832798212345a731649644c5dee9",
				"-p", "192.168.222.0/24:192.168.111.0/24:in:both",
				"-e", "192.168.222.2:192.168.111.2:aes:301:656c8523255ccc23a66c1917aa0cf309",
				"-a", "192.168.222.2:192.168.111.2:sha256:300:27f6d123d7077b361662fc6e451f65d88273567857462576a731649644c5dee9",
				"-c", "13", "-m", "ASYNC_IN_PLACE", NULL
			};

			for (int i = 0; i < 16; i++) {
				if ( i % 4 == 0 ) continue;
				boot_cluster(i, _argv[0], _argv);
			}
		}
		{
			static char const * _argv[] = {
				"odp_generator.kelf",
				"-I", "e0:loop:nofree",
				"--srcmac", "08:00:27:76:b5:e0",
				"--dstmac", "00:00:00:00:80:01",
				"--srcip",  "192.168.111.2",
				"--dstip", "192.168.222.2",
				"-m", "u",
				"-i", "0", "-w", "2", "-p", "256", NULL
			};

			for (int i = 0; i < 16; i+=4) {
				boot_cluster(i, _argv[0], _argv);
			}
		}
		printf("Cluster booted\n");
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
