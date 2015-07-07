/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */
#include <odp_packet_io_internal.h>
#include "HAL/hal/hal.h"
#include <odp/errno.h>
#include <errno.h>

#define MAX_ETH_SLOTS 2
#define MAX_ETH_PORTS 2

static int eth_global_init(void)
{
	return 0;
}

static int eth_init(pktio_entry_t * pktio_entry, odp_pool_t pool)
{
	pktio_eth_t * pkt_eth = &pktio_entry->s.eth;

	pkt_eth->pool = pool;

	return 0;
}

static int eth_open(pktio_entry_t * const pktio_entry, const char * devname)
{
	if (devname[0] != 'p')
		return -1;

	int slot_id = devname[1] - '0';
	int port_id = -1;
	if (slot_id < 0 || slot_id > MAX_ETH_SLOTS)
		return -1;

	if (devname[2] != 0) {
		if(devname[2] != 'p')
			return -1;
		port_id = devname[3] - '0';

		if (port_id < 0 || port_id > MAX_ETH_PORTS)
			return -1;

		if(devname[4] != 0)
			return -1;
	}

	pktio_entry->s.type = ODP_PKTIO_TYPE_ETH;
	snprintf(pktio_entry->s.eth.name, MAX_PKTIO_NAMESIZE, "%s", devname);

	pktio_entry->s.eth.slot_id = slot_id;
	pktio_entry->s.eth.port_id = port_id;
	return 0;
}

static void eth_mac_get(const pktio_entry_t *const pktio_entry ODP_UNUSED, void * mac_addr ODP_UNUSED)
{
	/* FIXME */
}

static int eth_recv(pktio_entry_t *const pktio_entry ODP_UNUSED, odp_packet_t pkt_table[] ODP_UNUSED, int len ODP_UNUSED)
{

	return 0;
}

static int eth_send(pktio_entry_t *const pktio_entry ODP_UNUSED, odp_packet_t pkt_table[] ODP_UNUSED, unsigned len ODP_UNUSED)
{
	return 0;
}

static int eth_promisc_mode_set(pktio_entry_t *const pktio_entry ODP_UNUSED, odp_bool_t enable ODP_UNUSED){
	return -1;
}

static int eth_promisc_mode(pktio_entry_t *const pktio_entry ODP_UNUSED){
	return -1;
}

static int eth_mtu_get(pktio_entry_t *const pktio_entry ODP_UNUSED) {
	return -1;
}
struct pktio_if_operation eth_pktio_operation = {
	.name = "eth",
	.global_init = eth_global_init,
	.setup_pktio_entry = eth_init,
	.mac_get = eth_mac_get,
	.recv = eth_recv,
	.send = eth_send,
	.promisc_mode_set = eth_promisc_mode_set,
	.promisc_mode_get = eth_promisc_mode,
	.mtu_get = eth_mtu_get,
	.open = eth_open,
};
