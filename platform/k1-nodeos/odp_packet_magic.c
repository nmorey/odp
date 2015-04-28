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
static int _magic_scall_get_mac(int id, void * mac){
	return __k1_syscall2(MAGIC_SCALL_ETH_GETMAC, id, (uint32_t)mac);
}
static int _magic_scall_recv(int id, void* packet){
	return __k1_syscall2(MAGIC_SCALL_ETH_RECV, id, (uint32_t)packet);
}
static int _magic_scall_send(int id, void * buf, unsigned len){
	return __k1_syscall3(MAGIC_SCALL_ETH_SEND, id, (uint32_t)buf, len);
}
int magic_global_init(void)
{
	return _magic_scall_init();
}

int magic_init(pktio_entry_t * pktio_entry, odp_pool_t pool)
{
	pkt_magic_t * pkt_magic = &pktio_entry->s.magic;
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
void magic_get_mac(const pkt_magic_t *const pkt_magic, void * mac_addr){
	_magic_scall_get_mac(pkt_magic->fd, mac_addr);
}
int magic_recv(pkt_magic_t *const pkt_magic, odp_packet_t pkt_table[], int len)
{
	ssize_t recv_bytes;
	int i;
	odp_packet_t pkt = ODP_PACKET_INVALID;
	uint8_t *pkt_buf;
	int nb_rx = 0;

	for (i = 0; i < len; i++) {
		if (odp_likely(pkt == ODP_PACKET_INVALID)) {
			pkt = odp_packet_alloc(pkt_magic->pool, pkt_magic->max_frame_len);
			if (odp_unlikely(pkt == ODP_PACKET_INVALID))
				break;
		}

		pkt_buf = odp_packet_data(pkt);

		recv_bytes = _magic_scall_recv(pkt_magic->fd, pkt_buf);

		/* no data or error: free recv buf and break out of loop */
		if (odp_unlikely(recv_bytes < 1))
			break;
		/* /\* frame not explicitly for us, reuse pkt buf for next frame *\/ */
		/* if (odp_unlikely(sll.sll_pkttype == PACKET_OUTGOING)) */
		/* 	continue; */

		/* Parse and set packet header data */
		odp_packet_pull_tail(pkt, pkt_magic->max_frame_len - recv_bytes);
		_odp_packet_parse(pkt);

		pkt_table[nb_rx] = pkt;
		pkt = ODP_PACKET_INVALID;
		nb_rx++;
	} /* end for() */

	if (odp_unlikely(pkt != ODP_PACKET_INVALID))
		odp_packet_free(pkt);

	return nb_rx;
}
int magic_send(pkt_magic_t *const pkt_magic, odp_packet_t pkt_table[], unsigned len)
{
	odp_packet_t pkt;
	uint8_t *frame;
	uint32_t frame_len;
	unsigned i;
	unsigned flags;
	int nb_tx;
	int ret;

	i = 0;
	while (i < len) {
		pkt = pkt_table[i];

		frame = odp_packet_l2_ptr(pkt, &frame_len);

		ret = _magic_scall_send(pkt_magic->fd, frame, frame_len);
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
