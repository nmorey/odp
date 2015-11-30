/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */
#include <odp.h>
#include <test_helper.h>

#include <odp/helper/eth.h>
#include <odp/helper/ip.h>
#include <odp/helper/udp.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <mppa_power.h>
#include <mppa_bsp.h>

#define PKT_BUF_NUM            256
#define PKT_BUF_SIZE           (2 * 1024)

#define PKT_SIZE		64

#define TEST_RUN_COUNT		1024

#define PCIE_INTERFACE_COUNT	16

odp_pool_t pool;
odp_pktio_t pktio;
odp_queue_t inq;

static int setup_test()
{
	odp_pool_param_t params;
	char pktio_name[] = "p0p0:tags=120";
	odp_pktio_param_t pktio_param = {0};

	memset(&params, 0, sizeof(params));
	params.pkt.seg_len = PKT_BUF_SIZE;
	params.pkt.len     = PKT_BUF_SIZE;
	params.pkt.num     = PKT_BUF_NUM;
	params.type        = ODP_POOL_PACKET;

	pool = odp_pool_create("pkt_pool_pcie", &params);
	if (ODP_POOL_INVALID == pool) {
		fprintf(stderr, "unable to create pool\n");
		return 1;
	}

	pktio_param.in_mode = ODP_PKTIN_MODE_POLL;

	pktio = odp_pktio_open(pktio_name, pool, &pktio_param);
	test_assert_ret(pktio != ODP_PKTIO_INVALID);

	test_assert_ret(odp_pktio_start(pktio) == 0);

	printf("Setup ok\n");
	return 0;
}

//~ static int term_test()
//~ {
	//~ test_assert_ret(odp_pktio_close(pktio) == 0);

	//~ test_assert_ret(odp_pool_destroy(pool) == 0);

	//~ return 0;
//~ }

static int run_pcie_simple()
{
	int ret;
	odp_packet_t packet;

	while (1) {
		ret = odp_pktio_recv(pktio, &packet, 1);

		test_assert_ret(ret >= 0);

		if (ret == 1)
			break;
	}
	printf("Received packet\n");
	test_assert_ret(odp_packet_is_valid(packet) == 1);

	odp_packet_free(packet);
	return 0;
}

extern int cluster_iopcie_sync(void);

int run_test()
{
	int i = 0;
	test_assert_ret(setup_test() == 0);

	for (i = 0; i < TEST_RUN_COUNT; i++) 
		test_assert_ret(run_pcie_simple() == 0);

	cluster_iopcie_sync();
	//~ test_assert_ret(term_test() == 0);

	return 0;
}

int main(int argc, char **argv)
{
	(void) argc;
	(void) argv;
	test_assert_ret(odp_init_global(NULL, NULL) == 0);
	test_assert_ret(odp_init_local(ODP_THREAD_CONTROL) == 0);

	test_assert_ret(run_test() == 0);

	//~ test_assert_ret(odp_term_local() == 0);
	//~ test_assert_ret(odp_term_global() == 0);

	mppa_power_base_exit(0);
	return 0;
}
