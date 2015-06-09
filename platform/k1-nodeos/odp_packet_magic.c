/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */
#include <odp_packet_io_internal.h>
#include "HAL/hal/hal.h"
#include <odp/errno.h>
#include <errno.h>
#include "../../syscall/include/common.h"

static int _magic_scall_init(void){
	return __k1_syscall0(MAGIC_SCALL_ETH_INIT);
}
static int _magic_scall_open(char * name, size_t len){
	return __k1_syscall2(MAGIC_SCALL_ETH_OPEN, (uint32_t)name, len);
}
static int _magic_scall_mac_get(int id, void * mac){
	return __k1_syscall2(MAGIC_SCALL_ETH_GETMAC, id, (uint32_t)mac);
}
static int _magic_scall_recv(int id, void* packet){
	return __k1_syscall2(MAGIC_SCALL_ETH_RECV, id, (uint32_t)packet);
}
static int _magic_scall_send(int id, void * buf, unsigned len){
	return __k1_syscall3(MAGIC_SCALL_ETH_SEND, id, (uint32_t)buf, len);
}
static int _magic_scall_prom_set(int id, int enable){
	return __k1_syscall2(MAGIC_SCALL_ETH_PROM_SET, id, enable);
}
static int _magic_scall_prom_get(int id){
	return __k1_syscall1(MAGIC_SCALL_ETH_PROM_GET, id);
}

int magic_global_init(void);
int magic_init(pktio_entry_t * pktio_entry, odp_pool_t pool);
void magic_mac_get(const pktio_entry_t *const pktio_entry, void * mac_addr);
int magic_recv(pktio_entry_t *const pktio_entry, odp_packet_t pkt_table[], int len);
int magic_send(pktio_entry_t *const pktio_entry, odp_packet_t pkt_table[], unsigned len);
int magic_promisc_mode_set(pktio_entry_t *const pktio_entry, odp_bool_t enable);
int magic_promisc_mode(pktio_entry_t *const pktio_entry);
int magic_open(pktio_entry_t * const pktio_entry, const char * devname);
int magic_mtu_get(pktio_entry_t *const pktio_entry);

int magic_global_init(void)
{
	return _magic_scall_init();
}

int magic_init(pktio_entry_t * pktio_entry, odp_pool_t pool)
{
	pktio_magic_t * pkt_magic = &pktio_entry->s.magic;
	pkt_magic->fd = _magic_scall_open(pkt_magic->name, strlen(pkt_magic->name));
	if(pkt_magic->fd < 0)
		return pkt_magic->fd;

	pkt_magic->pool = pool;
	/* pkt buffer size */
	pkt_magic->buf_size = odp_buffer_pool_segment_size(pool);
	/* max frame len taking into account the l2-offset */
	pkt_magic->max_frame_len = pkt_magic->buf_size -
		odp_buffer_pool_headroom(pool) -
		odp_buffer_pool_tailroom(pool);
	return 0;
}

int magic_open(pktio_entry_t * const pktio_entry, const char * devname)
{
	if(!strncmp("magic-", devname, strlen("magic-"))){
#ifndef MAGIC_SCALL
		(void)pktio_entry;
		ODP_ERR("Trying to invoke magic interface on H/W");
		return 1;
#else
		pktio_entry->s.type = ODP_PKTIO_TYPE_MAGIC;
		snprintf(pktio_entry->s.magic.name, MAX_PKTIO_NAMESIZE, "%s", devname + strlen("magic-"));
		return 0;
#endif
	}

	/* The device name does not match ours */
	return -1;
}

void magic_mac_get(const pktio_entry_t *const pktio_entry, void * mac_addr)
{
	_magic_scall_mac_get(pktio_entry->s.magic.fd, mac_addr);
}

int magic_recv(pktio_entry_t *const pktio_entry, odp_packet_t pkt_table[], int len)
{
	ssize_t recv_bytes;
	int i;
	odp_packet_t pkt = ODP_PACKET_INVALID;
	uint8_t *pkt_buf;
	int nb_rx = 0;

	for (i = 0; i < len; i++) {
		if (odp_likely(pkt == ODP_PACKET_INVALID)) {
			pkt = odp_packet_alloc(pktio_entry->s.magic.pool, pktio_entry->s.magic.max_frame_len);
			if (odp_unlikely(pkt == ODP_PACKET_INVALID))
				break;
		}

		pkt_buf = odp_packet_data(pkt);

		recv_bytes = _magic_scall_recv(pktio_entry->s.magic.fd, pkt_buf);

		/* no data or error: free recv buf and break out of loop */
		if (odp_unlikely(recv_bytes < 1))
			break;
		/* /\* frame not explicitly for us, reuse pkt buf for next frame *\/ */
		/* if (odp_unlikely(sll.sll_pkttype == PACKET_OUTGOING)) */
		/* 	continue; */

		/* Parse and set packet header data */
		odp_packet_pull_tail(pkt, pktio_entry->s.magic.max_frame_len - recv_bytes);
		_odp_packet_reset_parse(pkt);

		pkt_table[nb_rx] = pkt;
		pkt = ODP_PACKET_INVALID;
		nb_rx++;
	} /* end for() */

	if (odp_unlikely(pkt != ODP_PACKET_INVALID))
		odp_packet_free(pkt);

	return nb_rx;
}

int magic_send(pktio_entry_t *const pktio_entry, odp_packet_t pkt_table[], unsigned len)
{
	odp_packet_t pkt;
	uint8_t *frame;
	uint32_t frame_len;
	unsigned i;
	int nb_tx;
	int ret;

	i = 0;
	while (i < len) {
		pkt = pkt_table[i];

		frame = odp_packet_l2_ptr(pkt, &frame_len);

		ret = _magic_scall_send(pktio_entry->s.magic.fd, frame, frame_len);
		if (odp_unlikely(ret == -1)) {
			break;
		}

		i++;
	}			/* end while */
	nb_tx = i;

	for (i = 0; i < len; i++)
		odp_packet_free(pkt_table[i]);

	return nb_tx;
}

int magic_promisc_mode_set(pktio_entry_t *const pktio_entry, odp_bool_t enable){
	return _magic_scall_prom_set(pktio_entry->s.magic.fd, enable);
}

int magic_promisc_mode(pktio_entry_t *const pktio_entry){
	return _magic_scall_prom_get(pktio_entry->s.magic.fd);
}

int magic_mtu_get(__attribute__ ((unused)) pktio_entry_t *const pktio_entry) {
	return -1;
}

struct pktio_if_operation magic_pktio_operation = {
	.name = "magic",
	.global_init = magic_global_init,
	.setup_pktio_entry = magic_init,
	.mac_get = magic_mac_get,
	.recv = magic_recv,
	.send = magic_send,
	.promisc_mode_set = magic_promisc_mode_set,
	.promisc_mode_get = magic_promisc_mode,
	.mtu_get = magic_mtu_get,
	.open = magic_open,
};
