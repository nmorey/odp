/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */
#include <odp_packet_io_internal.h>
#include "HAL/hal/hal.h"
#include <odp/errno.h>
#include <errno.h>
#include "odp_rpc_internal.h"


#define MAX_ETH_SLOTS 2
#define MAX_ETH_PORTS 4


static int eth_init(void)
{
	return odp_rpc_client_setup();
}


static int eth_open(odp_pktio_t id ODP_UNUSED, pktio_entry_t *pktio_entry,
		    const char *devname, odp_pool_t pool)
{
	if (devname[0] != 'p')
		return -1;

	int slot_id = devname[1] - '0';
	int port_id = 4;
	if (slot_id < 0 || slot_id >= MAX_ETH_SLOTS)
		return -1;

	if (devname[2] != 0) {
		if(devname[2] != 'p')
			return -1;
		port_id = devname[3] - '0';

		if (port_id < 0 || port_id >= MAX_ETH_PORTS)
			return -1;

		if(devname[4] != 0)
			return -1;
	}

	pktio_entry->s.pkt_eth.slot_id = slot_id;
	pktio_entry->s.pkt_eth.port_id = port_id;
	pktio_entry->s.pkt_eth.pool = pool;
	return 0;
}

static int eth_close(pktio_entry_t * const pktio_entry ODP_UNUSED)
{
	return 0;
}

static int eth_mac_addr_get(pktio_entry_t *pktio_entry ODP_UNUSED,
			    void *mac_addr ODP_UNUSED)
{
	/* FIXME */
	return -1;
}

static int eth_recv(pktio_entry_t *pktio_entry ODP_UNUSED,
		    odp_packet_t pkt_table[] ODP_UNUSED,
		    unsigned len ODP_UNUSED)
{

	return 0;
}

static int eth_send(pktio_entry_t *pktio_entry ODP_UNUSED,
		    odp_packet_t pkt_table[] ODP_UNUSED,
		    unsigned len ODP_UNUSED)
{
	return 0;
}

static int eth_promisc_mode_set(pktio_entry_t *const pktio_entry ODP_UNUSED,
				odp_bool_t enable ODP_UNUSED){
	return -1;
}

static int eth_promisc_mode(pktio_entry_t *const pktio_entry ODP_UNUSED){
	return -1;
}

static int eth_mtu_get(pktio_entry_t *const pktio_entry ODP_UNUSED) {
	return -1;
}
const pktio_if_ops_t eth_pktio_ops = {
	.init = eth_init,
	.open = eth_open,
	.close = eth_close,
	.recv = eth_recv,
	.send = eth_send,
	.mtu_get = eth_mtu_get,
	.promisc_mode_set = eth_promisc_mode_set,
	.promisc_mode_get = eth_promisc_mode,
	.mac_get = eth_mac_addr_get,
};
