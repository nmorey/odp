/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */
#include <odp_packet_io_internal.h>
#include "HAL/hal/hal.h"
#include <odp/errno.h>
#include <errno.h>

#include <mppa_bsp.h>
#include <mppa_routing.h>
#include <mppa_noc.h>

#include <unistd.h>

#define DNOC_CLUS_BASE_RX	0

#define CNOC_CLUS_SYNC_RX_ID	16
#define CNOC_CLUS_BASE_RX_ID	0

#define NOC_CLUS_IFACE_ID	0

#define NOC_UC_COUNT		2

#define NOC_IODDR0_ID		128

#define ODP_PKTIO_MAX_PKT_SIZE		(1 * 512)
#define ODP_PKTIO_MAX_PKT_COUNT		5
#define ODP_PKTIO_PKT_BUF_SIZE		(ODP_PKTIO_MAX_PKT_COUNT * ODP_PKTIO_MAX_PKT_SIZE)

#ifdef __k1a__
	/// Mask of the destination offset to use for sending with absolute offset.
#define MPPA_NOC_UCORE_USE_ABSOLUTE_OFFSET	0xA0000000
#else
#define MPPA_NOC_UCORE_USE_ABSOLUTE_OFFSET	0xA000000000000000ULL
#endif

struct cluster_pkt_header {
	uint64_t pkt_size;
};

#define ODP_CLUS_DBG(fmt, ...)	ODP_DBG("[Clus %d] " fmt, __k1_get_cluster_id(), ##__VA_ARGS__)

/** \brief odp_ucode_linear
 * linear transfer
 * arg0: number of 64 bits elements
 * arg1: number of 8 bits elements (payload size)
 * arg2: destination offset
 * arg3: local offset
 */
extern unsigned long long odp_ucode_linear[] __attribute__((aligned(128)));

/**
 * Node availability
 */
static unsigned int g_available_clus_count;
static mppa_node_t g_available_clus[BSP_NB_CLUSTER_MAX];

static uint8_t *g_pkt_recv_buf[BSP_NB_CLUSTER_MAX];

/**
 * UCore status
 */
struct mppa_ethernet_uc_ctx {
	unsigned int dnoc_tx_id;
	unsigned int dnoc_uc_id;
	bool is_running;
	odp_packet_t pkt;
	struct cluster_pkt_header pkt_hdr;
};

static struct mppa_ethernet_uc_ctx g_uc_ctx[NOC_UC_COUNT] = {{0}};


static unsigned int g_cnoc_tx_id = 0;

/**
 * Fill the list of available clusters
 */
static inline void
mppacl_init_available_clusters(void)
{
	int i;

	/* Fill virtual node ids array */
	for (i = 0; i < BSP_NB_CLUSTER_MAX + BSP_NB_IOCLUSTER_MAX; i++) {
		if ( __bsp_platform[i].available) {
			if (__bsp_platform[i].cluster == node){
				g_available_clus[g_available_clus_count++] =
					__bsp_platform[i].node_id;
			}
		}
	}
}

static int cluster_init_dnoc_rx(int clus_id)
{
	mppa_noc_ret_t ret;
	mppa_noc_dnoc_rx_configuration_t conf = {0};

	g_pkt_recv_buf[clus_id] = malloc(ODP_PKTIO_PKT_BUF_SIZE);
	if (g_pkt_recv_buf[clus_id] == NULL)
		return 1;

	/* DNoC */
	ret = mppa_noc_dnoc_rx_alloc(NOC_CLUS_IFACE_ID,
				     DNOC_CLUS_BASE_RX + clus_id);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	conf.buffer_base = (uintptr_t) g_pkt_recv_buf[clus_id];
	conf.buffer_size = ODP_PKTIO_PKT_BUF_SIZE;
	conf.activation = MPPA_NOC_ACTIVATED;
	conf.reload_mode = MPPA_NOC_RX_RELOAD_MODE_INCR_DATA_NOTIF;

	ret = mppa_noc_dnoc_rx_configure(NOC_CLUS_IFACE_ID,
					 DNOC_CLUS_BASE_RX + clus_id, conf);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	return 0;
}

static int cluster_init_cnoc_rx(int clus_id)
{
#ifdef __k1b__
	mppa_cnoc_mailbox_notif_t notif = {0};
#endif
	mppa_noc_ret_t ret;
	mppa_noc_cnoc_rx_configuration_t conf = {0};
	int rx_id = CNOC_CLUS_BASE_RX_ID + clus_id;

	conf.mode = MPPA_NOC_CNOC_RX_MAILBOX;
	conf.init_value = 0;

	/* CNoC */
	ret = mppa_noc_cnoc_rx_alloc(NOC_CLUS_IFACE_ID, rx_id);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	ret = mppa_noc_cnoc_rx_configure(NOC_CLUS_IFACE_ID, rx_id, conf
#ifdef __k1b__
		, &notif
#endif
	);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	return 0;
}

static int cluster_init_io_cnoc_sync_rx(void)
{
#ifdef __k1b__
	mppa_cnoc_mailbox_notif_t notif = {0};
#endif
	mppa_noc_ret_t ret;
	mppa_noc_cnoc_rx_configuration_t conf = {0};

	conf.mode = MPPA_NOC_CNOC_RX_BARRIER;
	/* We only wait for a 1 value from IO */
	conf.init_value = ~1;

	/* CNoC */
	ret = mppa_noc_cnoc_rx_alloc(NOC_CLUS_IFACE_ID, CNOC_CLUS_SYNC_RX_ID);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	ret = mppa_noc_cnoc_rx_configure(NOC_CLUS_IFACE_ID, CNOC_CLUS_SYNC_RX_ID, conf
#ifdef __k1b__
		, &notif
#endif
	);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	return 0;
}

static int cluster_init_noc_rx(void)
{
	unsigned int i;
	int clus_id;

	for(i = 0; i < g_available_clus_count; i++) {
		clus_id = g_available_clus[i];

		if (cluster_init_dnoc_rx(clus_id))
			return 1;
		if (cluster_init_cnoc_rx(clus_id))
			return 1;
	}

	cluster_init_io_cnoc_sync_rx();

	return 0;
}

extern char _heap_end;

static int cluster_init_noc_tx(void)
{
	int i;
	mppa_noc_ret_t ret;
	mppa_noc_dnoc_uc_configuration_t uc_conf = MPPA_NOC_DNOC_UC_CONFIGURATION_INIT;

	uc_conf.program_start = (uintptr_t) odp_ucode_linear;
	uc_conf.buffer_base = (uintptr_t) &_data_start;
	uc_conf.buffer_size = (uintptr_t) &_heap_end - (uintptr_t) &_data_start;

	for (i = 0; i < NOC_UC_COUNT; i++) {

		/* DNoC */
		ret = mppa_noc_dnoc_tx_alloc_auto(NOC_CLUS_IFACE_ID, &g_uc_ctx[i].dnoc_tx_id, MPPA_NOC_BLOCKING);
		if (ret != MPPA_NOC_RET_SUCCESS)
			return 1;

		/* We will only use events */
		mppa_noc_disable_interrupt_handler(NOC_CLUS_IFACE_ID,
			MPPA_NOC_INTERRUPT_LINE_DNOC_TX, g_uc_ctx[i].dnoc_tx_id);

		ret = mppa_noc_dnoc_uc_alloc_auto(NOC_CLUS_IFACE_ID, &g_uc_ctx[i].dnoc_uc_id, MPPA_NOC_BLOCKING);
		if (ret != MPPA_NOC_RET_SUCCESS)
			return 1;

		ret = mppa_noc_dnoc_uc_link(NOC_CLUS_IFACE_ID, g_uc_ctx[i].dnoc_uc_id,
					    g_uc_ctx[i].dnoc_tx_id, uc_conf);
		if (ret != MPPA_NOC_RET_SUCCESS)
			return 1;

		g_uc_ctx[i].is_running = 0;		
	}

	/* CnoC */
	ret = mppa_noc_cnoc_tx_alloc_auto(NOC_CLUS_IFACE_ID, &g_cnoc_tx_id, MPPA_NOC_BLOCKING);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	return 0;
}

static int cluster_configure_cnoc_tx(int clus_id, int tag)
{
	mppa_noc_ret_t nret;
	mppa_routing_ret_t rret;
	mppa_cnoc_config_t config = {0};
	mppa_cnoc_header_t header = {0};

	rret = mppa_routing_get_cnoc_unicast_route(__k1_get_cluster_id(),
						   clus_id,
						   &config, &header);
	if (rret != MPPA_ROUTING_RET_SUCCESS)
		return 1;

	header._.tag = tag;

	nret = mppa_noc_cnoc_tx_configure(NOC_CLUS_IFACE_ID, g_cnoc_tx_id,
					  config, header);
	if (nret != MPPA_NOC_RET_SUCCESS)
		return 1;

	return 0;
}

static int cluster_io_sync(void)
{
	uint64_t value = 1 << __k1_get_cluster_id();

	if (cluster_configure_cnoc_tx(NOC_IODDR0_ID, CNOC_CLUS_SYNC_RX_ID) != 0)
		return 1;

	mppa_noc_cnoc_tx_push_eot(NOC_CLUS_IFACE_ID, g_cnoc_tx_id, value);

#ifdef __k1b__
	while(mppa_noc_cnoc_rx_get_value(NOC_CLUS_IFACE_ID, CNOC_CLUS_SYNC_RX_ID) != 0);
#else
	mppa_noc_wait_clear_event(NOC_CLUS_IFACE_ID, MPPA_NOC_INTERRUPT_LINE_CNOC_RX, CNOC_CLUS_SYNC_RX_ID);
#endif

	return 0;
}

static int cluster_init(void)
{
	mppacl_init_available_clusters();

	if (cluster_init_noc_rx())
		return 1;

	if (cluster_init_noc_tx())
		return 1;

	/* We need to sync only when spawning from another IO */
	if (__k1_spawn_type() == __MPPA_MPPA_SPAWN)
		cluster_io_sync();

	return 0;
}

static int cluster_open(odp_pktio_t id ODP_UNUSED, pktio_entry_t *pktio_entry,
			const char *devname, odp_pool_t pool)
{
	mppa_routing_ret_t rret;

	if(!strncmp("cluster:", devname, strlen("cluster:"))) {
		pkt_cluster_t * pkt_cluster = &pktio_entry->s.pkt_cluster;
		
		/* String should in the following format: "cluster:<cluster_id>" */
		pkt_cluster->clus_id = atoi(devname + strlen("cluster:"));

		if (pkt_cluster->clus_id < 0 || pkt_cluster->clus_id > 15)
			return -1;

		pkt_cluster->pool = pool;
		pkt_cluster->sent_pkt_count = 0;
		pkt_cluster->recv_pkt_count = 0;

		/* pkt buffer size */
		pkt_cluster->buf_size = odp_buffer_pool_segment_size(pool);
		/* max frame len taking into account the l2-offset */
		pkt_cluster->max_frame_len = pkt_cluster->buf_size -
			odp_buffer_pool_headroom(pool) -
			odp_buffer_pool_tailroom(pool);

		/* Get and configure route */
#ifdef __k1a__
		pkt_cluster->config.word = 0;
		pkt_cluster->config._.bandwidth = mppa_noc_dnoc_get_window_length(NOC_CLUS_IFACE_ID);
#else
		pkt_cluster->config._.loopback_multicast = 0;
		pkt_cluster->config._.cfg_pe_en = 1;
		pkt_cluster->config._.cfg_user_en = 1;
		pkt_cluster->config._.write_pe_en = 1;
		pkt_cluster->config._.write_user_en = 1;
		pkt_cluster->config._.decounter_id = 0;
		pkt_cluster->config._.decounted = 0;
		pkt_cluster->config._.payload_min = 0;
		pkt_cluster->config._.payload_max = 32;
		pkt_cluster->config._.bw_current_credit = 0xff;
		pkt_cluster->config._.bw_max_credit     = 0xff;
		pkt_cluster->config._.bw_fast_delay     = 0x00;
		pkt_cluster->config._.bw_slow_delay     = 0x00;
#endif

		pkt_cluster->header._.tag = DNOC_CLUS_BASE_RX + __k1_get_cluster_id();
		pkt_cluster->header._.valid = 1;

		rret = mppa_routing_get_dnoc_unicast_route(__k1_get_cluster_id(),
							   pkt_cluster->clus_id,
							   &pkt_cluster->config, &pkt_cluster->header);
		
		if (rret != MPPA_ROUTING_RET_SUCCESS) 
			return 1;
		return 0;
	}

	return -1;
}

static int cluster_close(pktio_entry_t * const pktio_entry ODP_UNUSED)
{
	return 0;
}


static int cluster_mac_addr_get(pktio_entry_t *pktio_entry,
				void *mac_addr)
{
	const pkt_cluster_t *pktio_clus = &pktio_entry->s.pkt_cluster;
	uint8_t *mac_addr_u = mac_addr;

	memset(mac_addr_u, 0, ETH_ALEN);

	mac_addr_u[0] = pktio_clus->clus_id;
	return ETH_ALEN;
}

static int cluster_send_recv_pkt_count(pkt_cluster_t *pktio_clus)
{
	if (cluster_configure_cnoc_tx(pktio_clus->clus_id, CNOC_CLUS_BASE_RX_ID + __k1_get_cluster_id()) != 0)
		return 1;

	mppa_noc_cnoc_tx_push(NOC_CLUS_IFACE_ID, g_cnoc_tx_id,
			      pktio_clus->recv_pkt_count);

	return 0;
}


static int cluster_receive_single_packet(pkt_cluster_t *pktio_clus,
					 odp_packet_t *pkt)
{
	uint8_t *pkt_buf, *buf;
	ssize_t recv_bytes;
	struct cluster_pkt_header *pkt_header;
	int pkt_slot = pktio_clus->recv_pkt_count % ODP_PKTIO_MAX_PKT_COUNT;

	*pkt = odp_packet_alloc(pktio_clus->pool, pktio_clus->max_frame_len);
	if (odp_unlikely(*pkt == ODP_PACKET_INVALID))
		return 1;

	pkt_buf = odp_packet_data(*pkt);

	__k1_mb();

	buf = g_pkt_recv_buf[pktio_clus->clus_id] + (pkt_slot * ODP_PKTIO_MAX_PKT_SIZE);

	pkt_header = (struct cluster_pkt_header *) buf;
	recv_bytes = pkt_header->pkt_size;
	ODP_CLUS_DBG("Received packet of %d bytes in slot %d\n",
	       recv_bytes, pkt_slot);

	/* no data or error: free recv buf and break out of loop */
	if (odp_unlikely(recv_bytes < 1)) {
		odp_packet_free(*pkt);
		return 1;
	}

	memcpy(pkt_buf, buf + sizeof(struct cluster_pkt_header), recv_bytes);

	/* /\* frame not explicitly for us, reuse pkt buf for next frame *\/ */
	/* if (odp_unlikely(sll.sll_pkttype == PACKET_OUTGOING)) */
	/* 	continue; */

	/* Parse and set packet header data */
	odp_packet_pull_tail(*pkt, pktio_clus->max_frame_len - recv_bytes);
	_odp_packet_reset_parse(*pkt);

	pktio_clus->recv_pkt_count++;

	if (cluster_send_recv_pkt_count(pktio_clus) != 0)
		return 1;

	return 0;
}

static int cluster_recv(pktio_entry_t *const pktio_entry ODP_UNUSED,
			odp_packet_t pkt_table[] ODP_UNUSED,
			unsigned len ODP_UNUSED)
{
	unsigned int nb_rx = 0;
	int ret;
	uint16_t item_cnt;
	pkt_cluster_t *pktio_clus = &pktio_entry->s.pkt_cluster;
	odp_packet_t pkt;

	/* Check if we received some packets */
	item_cnt = mppa_noc_dnoc_rx_lac_event_counter(NOC_CLUS_IFACE_ID,  DNOC_CLUS_BASE_RX +
							  pktio_clus->clus_id);

	/* FIXME, we need to store if there are more packets than available space on our side */
	if (item_cnt == 0)
		return 0;

	for (nb_rx = 0; nb_rx < item_cnt; nb_rx++) {
		ret = cluster_receive_single_packet(pktio_clus, &pkt);
		if (ret != 0)
			return nb_rx;

		pkt_table[nb_rx] = pkt;
	}

	return nb_rx;
}

static inline int
cluster_send_single_packet(pkt_cluster_t *pktio_clus,
			   odp_packet_t pkt)
{
	mppa_noc_dnoc_uc_configuration_t uc_conf = MPPA_NOC_DNOC_UC_CONFIGURATION_INIT;

	mppa_noc_uc_program_run_t program_run = {{1, 1}};
	mppa_noc_ret_t nret;
	uint64_t remote_pkt_count;
	uintptr_t remote_offset;
	int tx_index = pktio_clus->clus_id % NOC_UC_COUNT;
	odp_packet_hdr_t * pkt_hdr = odp_packet_hdr(pkt);
	void *pkt_addr = packet_map(pkt_hdr, 0, NULL);

#ifdef __k1a__
	mppa_noc_event_line_t event_line;

	/* Event config */
	event_line.line = MPPA_NOC_USE_EVENT;
	event_line.pe_mask = 1 << __k1_get_cpu_id();
#endif
	uc_conf.pointers = NULL;
	uc_conf.event_counter = 0;

	/* Get credits first */
	remote_pkt_count = mppa_noc_cnoc_rx_get_value(NOC_CLUS_IFACE_ID,
						      CNOC_CLUS_BASE_RX_ID +
						      pktio_clus->clus_id);

	/* Is there enough room to send a packet ? */
	if ((pktio_clus->sent_pkt_count - remote_pkt_count) >=
	    ODP_PKTIO_MAX_PKT_COUNT) {
		return 1;
	}

	remote_offset = (pktio_clus->sent_pkt_count % ODP_PKTIO_MAX_PKT_COUNT) *
		ODP_PKTIO_MAX_PKT_SIZE;

	if (g_uc_ctx[tx_index].is_running) {

		mppa_noc_wait_clear_event(NOC_CLUS_IFACE_ID,
					  MPPA_NOC_INTERRUPT_LINE_DNOC_TX,
					  g_uc_ctx[tx_index].dnoc_tx_id);

		odp_packet_free(g_uc_ctx[tx_index].pkt);
	}

	ODP_CLUS_DBG("Sending iovec len 0x%08x, addr: %p, offset: %d, tx_index: %d, header\n", pkt_hdr->frame_len, pkt_addr, remote_offset, tx_index);

	/* Fill informations for transfer */
	g_uc_ctx[tx_index].pkt = pkt;
	g_uc_ctx[tx_index].pkt_hdr.pkt_size = pkt_hdr->frame_len;
	

	uc_conf.parameters[0] = pkt_hdr->frame_len / sizeof(uint64_t);
	uc_conf.parameters[1] = pkt_hdr->frame_len % sizeof(uint64_t);
	uc_conf.parameters[2] = remote_offset | MPPA_NOC_UCORE_USE_ABSOLUTE_OFFSET;
	uc_conf.parameters[3] = (uintptr_t) pkt_addr - (uintptr_t) &_data_start;
	/* Header size and address */
	assert((sizeof(struct cluster_pkt_header) % sizeof(uint64_t)) == 0);
	uc_conf.parameters[4] = sizeof(struct cluster_pkt_header) / sizeof(uint64_t);
	uc_conf.parameters[5] = (uintptr_t) &g_uc_ctx[tx_index].pkt_hdr - (uintptr_t) &_data_start;

	nret = mppa_noc_dnoc_uc_configure(NOC_CLUS_IFACE_ID,  g_uc_ctx[tx_index].dnoc_uc_id,
					  uc_conf, pktio_clus->header, pktio_clus->config
#ifdef __k1a__
		, event_line
#endif
	);
	if (nret != MPPA_NOC_RET_SUCCESS)
		return 1;

	g_uc_ctx[tx_index].is_running = 1;
	mppa_noc_dnoc_uc_set_program_run(NOC_CLUS_IFACE_ID, g_uc_ctx[tx_index].dnoc_uc_id,
					 program_run);

	/* Modify offset for next package */
	pktio_clus->sent_pkt_count++;

	return 0;
}

static int cluster_send(pktio_entry_t *const pktio_entry,
			odp_packet_t pkt_table[], unsigned len)
{
	pkt_cluster_t *pktio_clus = &pktio_entry->s.pkt_cluster;
	odp_packet_t pkt;
	unsigned i;
	int nb_tx = 0;

	ODP_CLUS_DBG("Sending %d packet(s)\n", len);

	for (i = 0; i < len; i++) {
		pkt = pkt_table[i];

		if(cluster_send_single_packet(pktio_clus, pkt))
			break;
	}
	nb_tx = i;

	return nb_tx;
}

static int cluster_promisc_mode_set(pktio_entry_t *const pktio_entry ODP_UNUSED,
				    odp_bool_t enable ODP_UNUSED)
{
	return 0;
}

static int cluster_promisc_mode(pktio_entry_t *const pktio_entry ODP_UNUSED)
{
	return 0;
}

static int cluster_mtu_get(pktio_entry_t *const pktio_entry ODP_UNUSED)
{
	return ODP_PKTIO_MAX_PKT_SIZE;
}

const pktio_if_ops_t cluster_pktio_ops = {
	.init = cluster_init,
	.term = NULL,
	.open = cluster_open,
	.close = cluster_close,
	.start = NULL,
	.stop = NULL,
	.recv = cluster_recv,
	.send = cluster_send,
	.mtu_get = cluster_mtu_get,
	.promisc_mode_set = cluster_promisc_mode_set,
	.promisc_mode_get = cluster_promisc_mode,
	.mac_get = cluster_mac_addr_get,
};
