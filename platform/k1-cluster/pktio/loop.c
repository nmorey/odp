/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */
#include <odp_packet_io_internal.h>
#include "HAL/hal/hal.h"
#include <odp/errno.h>
#include <errno.h>


/* MTU to be reported for the "loop" interface */
#define PKTIO_LOOP_MTU 1500

/* MAC address for the "loop" interface */
static const char pktio_loop_mac[] = {0x02, 0xe9, 0x34, 0x80, 0x73, 0x01};

static int loop_open(pktio_entry_t * const pktio_entry, const char * devname)
{
	if(!strncmp("loop", devname, strlen("loop"))){
		pktio_entry->s.type = ODP_PKTIO_TYPE_LOOPBACK;
		return 0;
	}

	return -1;
}

static int loop_close(pktio_entry_t * const pktio_entry)
{
	pktio_loopback_t * pkt_loop = &pktio_entry->s.loop;
	return odp_queue_destroy(pkt_loop->loopq);
}

static int loop_init(pktio_entry_t *entry, __attribute__ ((unused)) odp_pool_t pool)
{
	char loopq_name[ODP_QUEUE_NAME_LEN];
	odp_pktio_t id = entry->s.handle;

	snprintf(loopq_name, sizeof(loopq_name), "%" PRIu64 "-pktio_loopq",
		 odp_pktio_to_u64(id));
	entry->s.loop.loopq = odp_queue_create(loopq_name,
					  ODP_QUEUE_TYPE_POLL, NULL);

	if (entry->s.loop.loopq == ODP_QUEUE_INVALID)
		return -1;

	return 0;
}


static void loop_mac_get(__attribute__((unused)) const pktio_entry_t *const pktio_entry, void * mac_addr)
{
	memcpy(mac_addr, pktio_loop_mac, ETH_ALEN);
}

static int loop_recv(pktio_entry_t *const pktio_entry, odp_packet_t pkt_table[], int len)
{
	int nbr, i;
	odp_buffer_hdr_t *hdr_tbl[QUEUE_MULTI_MAX];
	queue_entry_t *qentry;

	qentry = queue_to_qentry(pktio_entry->s.loop.loopq);
	nbr = queue_deq_multi(qentry, hdr_tbl, len);

	for (i = 0; i < nbr; ++i) {
		pkt_table[i] = _odp_packet_from_buffer(odp_hdr_to_buf(hdr_tbl[i]));
		_odp_packet_reset_parse(pkt_table[i]);
	}

	return nbr;
}
static int loop_send(pktio_entry_t *const pktio_entry, odp_packet_t pkt_table[], unsigned len)
{
	odp_buffer_hdr_t *hdr_tbl[QUEUE_MULTI_MAX];
	queue_entry_t *qentry;
	unsigned i;

	for (i = 0; i < len; ++i)
		hdr_tbl[i] = odp_buf_to_hdr(_odp_packet_to_buffer(pkt_table[i]));

	qentry = queue_to_qentry(pktio_entry->s.loop.loopq);
	return queue_enq_multi(qentry, hdr_tbl, len);
}

static int loop_promisc_mode_set(__attribute__((unused)) pktio_entry_t *const pktio_entry,
			  __attribute__((unused)) odp_bool_t enable)
{
	return 0;
}

static int loop_promisc_mode(pktio_entry_t *const pktio_entry)
{
	return pktio_entry->s.promisc ? 1 : 0;
}

static int loop_mtu_get(__attribute__((unused)) pktio_entry_t *const pktio_entry)
{
	return PKTIO_LOOP_MTU;
}

struct pktio_if_operation loop_pktio_operation = {
	.name = "loop",
	.global_init = NULL,
	.setup_pktio_entry = loop_init,
	.mac_get = loop_mac_get,
	.recv = loop_recv,
	.send = loop_send,
	.promisc_mode_set = loop_promisc_mode_set,
	.promisc_mode_get = loop_promisc_mode,
	.mtu_get = loop_mtu_get,
	.open = loop_open,
	.close = loop_close,
};
