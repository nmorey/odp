/* Copyright (c) 2014, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */
#include <odp.h>
#include <odp_cunit_common.h>

#include <odp/helper/eth.h>
#include <odp/helper/ip.h>
#include <odp/helper/udp.h>

#include <stdlib.h>

#define PKT_BUF_NUM            8
#define PKT_BUF_SIZE           (2 * 1024)
#define PKT_LEN_NORMAL         64
#define PKT_LEN_JUMBO          (PKT_BUF_SIZE - ODPH_ETHHDR_LEN - \
				ODPH_IPV4HDR_LEN - ODPH_UDPHDR_LEN)

#define MAX_NUM_IFACES         8
#define IFACE_NAME_SIZE		14
#define TEST_SEQ_INVALID       ((uint32_t)~0)
#define TEST_SEQ_MAGIC         0x92749451

/** interface names used for testing */
static char iface_name[MAX_NUM_IFACES][IFACE_NAME_SIZE];

/** number of interfaces being used (1=loopback, 2=pair) */
static int num_ifaces = MAX_NUM_IFACES;

/** default packet pool */
odp_pool_t default_pkt_pool = ODP_POOL_INVALID;

odp_pool_t pool[MAX_NUM_IFACES] = {ODP_POOL_INVALID, ODP_POOL_INVALID};


static int default_pool_create(void)
{
	odp_pool_param_t params;

	if (default_pkt_pool != ODP_POOL_INVALID)
		return -1;

	memset(&params, 0, sizeof(params));
	params.pkt.seg_len = PKT_BUF_SIZE;
	params.pkt.len     = PKT_BUF_SIZE;
	params.pkt.num     = PKT_BUF_NUM;
	params.type        = ODP_POOL_PACKET;

	default_pkt_pool = odp_pool_create("pkt_pool_default",
					   &params);
	if (default_pkt_pool == ODP_POOL_INVALID)
		return -1;

	return 0;
}

static odp_pktio_t create_pktio(const char *iface, int num)
{
	odp_pktio_t pktio;

	pktio = odp_pktio_open(iface, pool[num]);
	if (pktio == ODP_PKTIO_INVALID)
		pktio = odp_pktio_lookup(iface);
	CU_ASSERT(pktio != ODP_PKTIO_INVALID);
	CU_ASSERT(odp_pktio_to_u64(pktio) !=
		  odp_pktio_to_u64(ODP_PKTIO_INVALID));

	return pktio;
}

static void pktio_test_mtu(void)
{
	int ret;
	int mtu;
	odp_pktio_t pktio = create_pktio(iface_name[0], 0);

	mtu = odp_pktio_mtu(pktio);
	CU_ASSERT(mtu > 0);

	printf(" %d ",  mtu);

	ret = odp_pktio_close(pktio);
	CU_ASSERT(ret == 0);
}

static void pktio_test_mac(void)
{
	unsigned char mac_addr[ODPH_ETHADDR_LEN];
	int mac_len, i;
	odp_pktio_t pktio;

	printf("testing mac for %s\n", iface_name[0]);

	for (i = 0; i < MAX_NUM_IFACES; i++) {
		pktio = create_pktio(iface_name[i], 0);
	
		mac_len = odp_pktio_mac_addr(pktio, mac_addr, sizeof(mac_addr));
		CU_ASSERT(ODPH_ETHADDR_LEN == mac_len);
		CU_ASSERT(mac_addr[0] == i);
		
	}

	/* Fail case: wrong addr_size. Expected <0. */
	mac_len = odp_pktio_mac_addr(pktio, mac_addr, 2);
	CU_ASSERT(mac_len < 0);

}

static void pktio_test_open(void)
{
	odp_pktio_t pktio;
	int i;

	for (i = 0; i < MAX_NUM_IFACES; i++) {
		printf("Opening pktio %s\n", iface_name[i]);
		pktio = odp_pktio_open(iface_name[i], default_pkt_pool);
		CU_ASSERT(pktio != ODP_PKTIO_INVALID);
		CU_ASSERT(odp_pktio_close(pktio) == 0);
		printf("Close pktio %s ok\n", iface_name[i]);
	}

	pktio = odp_pktio_open("cluster:16", default_pkt_pool);
	CU_ASSERT(pktio == ODP_PKTIO_INVALID);
}

static void pktio_test_lookup(void)
{
	odp_pktio_t pktio, pktio_inval;
	int i;

	for (i = 0; i < MAX_NUM_IFACES; i++) {
		pktio = odp_pktio_open(iface_name[0], default_pkt_pool);
		CU_ASSERT(pktio != ODP_PKTIO_INVALID);

		CU_ASSERT(odp_pktio_lookup(iface_name[0]) == pktio);

		pktio_inval = odp_pktio_open(iface_name[0], default_pkt_pool);
		CU_ASSERT(odp_errno() != 0);
		CU_ASSERT(pktio_inval == ODP_PKTIO_INVALID);

		CU_ASSERT(odp_pktio_close(pktio) == 0);

		CU_ASSERT(odp_pktio_lookup(iface_name[0]) == ODP_PKTIO_INVALID);
	}
}


static int create_pool(const char *iface, int num)
{
	char pool_name[ODP_POOL_NAME_LEN];
	odp_pool_param_t params;

	memset(&params, 0, sizeof(params));
	params.pkt.seg_len = PKT_BUF_SIZE;
	params.pkt.len     = PKT_BUF_SIZE;
	params.pkt.num     = PKT_BUF_NUM;
	params.type        = ODP_POOL_PACKET;

	snprintf(pool_name, sizeof(pool_name), "pkt_pool_%s", iface);

	pool[num] = odp_pool_create(pool_name, &params);
	if (ODP_POOL_INVALID == pool[num]) {
		fprintf(stderr, "unable to create pool\n");
		return -1;
	}
	printf("Open %s ok\n", iface);

	return 0;
}

static int pktio_suite_init(void)
{
	int i;

	printf("Init testsuite\n");
	for (i = 0; i < MAX_NUM_IFACES; i++) {
		pool[i] = ODP_POOL_INVALID;

		sprintf(iface_name[i], "cluster:%d", i);
		if (create_pool(iface_name[i], i) != 0)
			return -1;
	}

	if (default_pool_create() != 0) {
		fprintf(stderr, "error: failed to create default pool\n");
		return -1;
	}

	printf("init ok\n");
	return 0;
}

static int pktio_suite_term(void)
{
	char pool_name[ODP_POOL_NAME_LEN];
	odp_pool_t pool;
	int i;
	int ret = 0;

	for (i = 0; i < num_ifaces; ++i) {
		snprintf(pool_name, sizeof(pool_name),
			 "pkt_pool_%s", iface_name[i]);
		pool = odp_pool_lookup(pool_name);
		if (pool == ODP_POOL_INVALID)
			continue;

		if (odp_pool_destroy(pool) != 0) {
			fprintf(stderr, "error: failed to destroy pool %s\n",
				pool_name);
			ret = -1;
		}
	}

	if (odp_pool_destroy(default_pkt_pool) != 0) {
		fprintf(stderr, "error: failed to destroy default pool\n");
		ret = -1;
	}

	return ret;
}

static CU_TestInfo pktio_suite[] = {
	{"pktio open",		pktio_test_open},
	{"pktio lookup",	pktio_test_lookup},
	{"pktio mac",		pktio_test_mac},
	{"pktio mtu",		pktio_test_mtu},
	CU_TEST_INFO_NULL
};

static CU_SuiteInfo pktio_suites[] = {
	{"Packet I/O",
		pktio_suite_init, pktio_suite_term, NULL, NULL, pktio_suite},
	CU_SUITE_INFO_NULL
};

int main(int argc, char **argv)
{
	return odp_cunit_run(pktio_suites);
}
