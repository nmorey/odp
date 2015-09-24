#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>
#include <string.h>

#include "lib_trng.h"
#include "odp_rpc_internal.h"
#include "odp_macros_internal.h"
#include "rpc-server.h"

#if 0	//	AB01
#define TRNG_PARAM_NOISE 12
#define TRNG_PARAM_DIV   4
#define TRNG_PARAM_CYCLE 8
#else	//	KONIC
#define TRNG_PARAM_NOISE 12
#define TRNG_PARAM_DIV   15
#define TRNG_PARAM_CYCLE 4
#endif

void
odp_rnd_gen_init() {
	mppa_trng_config_simple(TRNG_PARAM_NOISE, TRNG_PARAM_DIV, TRNG_PARAM_CYCLE);
	mppa_trng_enable();
}


int
odp_rnd_gen_get(char *buf, unsigned len) {
	(void)buf;

	unsigned curr_len = 0;
	while ( curr_len < len ) {
		mppa_trng_rnd_data_t random_data = {{0}};
		if (mppa_trng_failure_event()) {
			printf("failure detected\n");
			mppa_trng_repare_failure(TRNG_PARAM_NOISE, TRNG_PARAM_DIV, TRNG_PARAM_CYCLE);
		}
		while (!mppa_trng_data_ready()) {};

		int data_status = mppa_trng_read_data(&random_data);

		if (!data_status) {
			printf("invalid random data\n");
			printf("trng int status: %x\n", mppa_trng_read_status());
			exit(1);
		}
		mppa_trng_ack_data();
		unsigned copy_len = MIN(len - curr_len, sizeof(random_data.data));
		memcpy(buf, random_data.data, copy_len);
		buf += copy_len;
		curr_len += copy_len;
	}
	return len;
}

void
rnd_send_buffer(unsigned remoteClus, odp_rpc_t * msg) {
	(void)remoteClus;
	odp_rpc_cmd_rnd_t rnd = (odp_rpc_cmd_rnd_t)msg->inl_data;
	assert( rnd.rnd_len <= sizeof(msg->inl_data.data));
	odp_rnd_gen_get((char*)(msg->inl_data.data), rnd.rnd_len);

	unsigned interface = 0;

	odp_rpc_send_msg(interface, msg->dma_id, msg->dnoc_tag, msg, NULL);
}

static int rnd_rpc_handler(unsigned remoteClus, odp_rpc_t *msg, uint8_t *payload)
{
	(void)payload;
	switch (msg->pkt_type){
	case ODP_RPC_CMD_RND_GET:
		rnd_send_buffer(remoteClus, msg);
		break;
	default:
		return -1;
	}
	return 0;
}

void  __attribute__ ((constructor)) __rnd_rpc_constructor()
{
	odp_rnd_gen_init();
	if(__n_rpc_handlers < MAX_RPC_HANDLERS) {
		__rpc_handlers[__n_rpc_handlers++] = rnd_rpc_handler;
	} else {
		fprintf(stderr, "Failed to register RND RPC handlers\n");
		exit(EXIT_FAILURE);
	}
}
