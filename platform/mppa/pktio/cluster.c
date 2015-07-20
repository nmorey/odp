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
#define DNOC_CLUS_TX_ID		0
#define DNOC_CLUS_UC_ID		0

#define CNOC_CLUS_SYNC_RX_ID	16
#define CNOC_CLUS_TX_ID		0
#define CNOC_CLUS_BASE_RX_ID	0

#define NOC_CLUS_IFACE_ID	0


#define NOC_IODDR0_ID		128

#define ODP_PKTIO_MAX_PKT_SIZE		(1 * 512)
#define ODP_PKTIO_MAX_PKT_COUNT		5
#define ODP_PKTIO_PKT_BUF_SIZE		(ODP_PKTIO_MAX_PKT_COUNT * ODP_PKTIO_MAX_PKT_SIZE)

struct cluster_pkt_header {
	uint32_t pkt_size;
};

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
static volatile unsigned int g_uc_is_running = 0;

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

	//~ mppa_noc_register_interrupt_handler(NOC_CLUS_IFACE_ID, MPPA_NOC_INTERRUPT_LINE_DNOC_RX,
		//~ rx_id, 0xFFFF, mppa_noc_callback_t callback, void *arg);

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

extern char _heap_start, _heap_end;

static int cluster_init_noc_tx(void)
{
	mppa_noc_ret_t ret;
	mppa_noc_dnoc_uc_configuration_t uc_conf;

	uc_conf.program_start = (uintptr_t) odp_ucode_linear;
	uc_conf.buffer_base = (uintptr_t) &_heap_start;
	uc_conf.buffer_size = &_heap_end - &_heap_start;

	/* We will only use events */
	mppa_noc_disable_interrupt_handler(NOC_CLUS_IFACE_ID,
		MPPA_NOC_INTERRUPT_LINE_DNOC_TX, DNOC_CLUS_TX_ID);

	/* DNoC */
	ret = mppa_noc_dnoc_tx_alloc(NOC_CLUS_IFACE_ID, DNOC_CLUS_TX_ID);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	ret = mppa_noc_dnoc_uc_alloc(NOC_CLUS_IFACE_ID, DNOC_CLUS_UC_ID);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	ret = mppa_noc_dnoc_uc_link(NOC_CLUS_IFACE_ID, DNOC_CLUS_UC_ID,
				    DNOC_CLUS_TX_ID, uc_conf);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	/* CnoC */
	ret = mppa_noc_cnoc_tx_alloc(NOC_CLUS_IFACE_ID, CNOC_CLUS_TX_ID);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	return 0;
}

static int cluster_configure_cnoc_tx(int clus_id)
{
	mppa_noc_ret_t nret;
	mppa_routing_ret_t rret;
	mppa_cnoc_config_t config;
	mppa_cnoc_header_t header;

	rret = mppa_routing_get_cnoc_unicast_route(__k1_get_cluster_id(),
						   clus_id,
						   &config, &header);
	if (rret != MPPA_ROUTING_RET_SUCCESS)
		return 1;

	header._.tag = CNOC_CLUS_BASE_RX_ID + clus_id;

	nret = mppa_noc_cnoc_tx_configure(NOC_CLUS_IFACE_ID, CNOC_CLUS_TX_ID,
					  config, header);
	if (nret != MPPA_NOC_RET_SUCCESS)
		return 1;

	return 0;
}

static int cluster_io_sync(void)
{
	uint64_t value = 1 << __k1_get_cluster_id();
	ODP_DBG("Signaling IO for sync\n");

	if (cluster_configure_cnoc_tx(NOC_IODDR0_ID) != 0)
		return 1;

	mppa_noc_cnoc_tx_push_eot(NOC_CLUS_IFACE_ID, CNOC_CLUS_TX_ID, value);
	ODP_DBG("Waiting for IO sync\n");

	while ((volatile bool) mppa_noc_has_pending_event(NOC_CLUS_IFACE_ID, MPPA_NOC_INTERRUPT_LINE_CNOC_RX) !=
			true);

	mppa_noc_cnoc_clear_rx_event(NOC_CLUS_IFACE_ID, CNOC_CLUS_SYNC_RX_ID);

	ODP_DBG("IO sync ok\n");

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

#ifdef __k1a__
static int cluster_send_recv_pkt_count(pkt_cluster_t *pktio_clus)
{
	if (cluster_configure_cnoc_tx(pktio_clus->clus_id) != 0)
		return 1;

	mppa_noc_cnoc_tx_push(NOC_CLUS_IFACE_ID, CNOC_CLUS_TX_ID,
			      pktio_clus->recv_pkt_count);

	return 0;
}
#endif

#ifdef __k1a__
static int cluster_receive_single_packet(pkt_cluster_t *pktio_clus,
					 odp_packet_t *pkt)
{
	uint8_t *pkt_buf;
	ssize_t recv_bytes;
	struct cluster_pkt_header *pkt_header;
	int pkt_slot = pktio_clus->recv_pkt_count % ODP_PKTIO_MAX_PKT_COUNT;

	*pkt = odp_packet_alloc(pktio_clus->pool, pktio_clus->max_frame_len);
	if (odp_unlikely(*pkt == ODP_PACKET_INVALID))
		return 1;

	pkt_buf = odp_packet_data(*pkt);

	pkt_header = (struct cluster_pkt_header *) g_pkt_recv_buf[pkt_slot];
	recv_bytes = pkt_header->pkt_size;
	printf("Received packet of %zd bytes in slot %d\n",
	       recv_bytes, pkt_slot);

	memcpy(pkt_buf, g_pkt_recv_buf[pkt_slot] +
	       sizeof(struct cluster_pkt_header), recv_bytes);

	/* no data or error: free recv buf and break out of loop */
	if (odp_unlikely(recv_bytes < 1)) {
		odp_packet_free(*pkt);
		return 1;
	}

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
#endif

static int cluster_recv(pktio_entry_t *const pktio_entry ODP_UNUSED,
			odp_packet_t pkt_table[] ODP_UNUSED,
			unsigned len ODP_UNUSED)
{
	unsigned int nb_rx = 0;
#ifdef __k1a__
	mppa_noc_dnoc_rx_counters_t counter;
	pkt_cluster_t *pktio_clus = &pktio_entry->s.pkt_cluster;
	odp_packet_t pkt;

	/* Check if we received some packets */
	counter = mppa_noc_dnoc_rx_lac_item_event_counter(NOC_CLUS_IFACE_ID,
							  DNOC_CLUS_BASE_RX +
							  pktio_clus->clus_id);
	if (counter.event_counter == 0)
		return 0;

	printf("%ld packet(s) available\n", counter.event_counter);

	for (nb_rx = 0; nb_rx < counter.event_counter; nb_rx++) {
		cluster_receive_single_packet(pktio_clus, &pkt);
		pkt_table[nb_rx] = pkt;
	}
#endif

	return nb_rx;
}

static inline int
cluster_send_single_packet(pkt_cluster_t *pktio_clus,
			   void *frame, uint32_t frame_len)
{
	mppa_noc_dnoc_uc_configuration_t uc_conf;
	mppa_dnoc_channel_config_t config = { 0 };
	mppa_dnoc_header_t header = { 0 };

	mppa_noc_uc_program_run_t program_run;
	mppa_noc_ret_t nret;
	mppa_routing_ret_t rret;
	uint64_t remote_pkt_count;
	uintptr_t remote_offset;
	uint8_t *tmp_pkt;
	struct cluster_pkt_header pkt_header;
#ifdef __k1a__
	mppa_noc_event_line_t event_line;

	/* Event config */
	event_line.line = MPPA_NOC_USE_EVENT;
	event_line.pe_mask = __k1_get_cpu_id();
#endif
	uc_conf.pointers = NULL;
	uc_conf.event_counter = 0;

	/* Get credits first */
	remote_pkt_count = mppa_noc_cnoc_rx_get_value(NOC_CLUS_IFACE_ID,
						      CNOC_CLUS_BASE_RX_ID +
						      pktio_clus->clus_id);

	/* Is there enough room to send a packet ? */
	if ((pktio_clus->sent_pkt_count - remote_pkt_count) >=
	    ODP_PKTIO_MAX_PKT_COUNT)
		return 1;


	/* Get and configure route */
#ifdef __k1a__
	config.word = 0;
	config._.bandwidth = mppa_noc_dnoc_get_window_length(NOC_CLUS_IFACE_ID);
#endif

	header._.tag = DNOC_CLUS_BASE_RX + __k1_get_cluster_id();

	rret = mppa_routing_get_dnoc_unicast_route(__k1_get_cluster_id(),
						   pktio_clus->clus_id,
						   &config, &header);
	if (rret != MPPA_ROUTING_RET_SUCCESS) 
		return 1;

	tmp_pkt = malloc(ODP_PKTIO_MAX_PKT_SIZE +
			 sizeof(struct cluster_pkt_header));
	if (!tmp_pkt)
		return 1;

	pkt_header.pkt_size = frame_len;
	memcpy(tmp_pkt, &pkt_header, sizeof(struct cluster_pkt_header));
	memcpy(tmp_pkt + sizeof(struct cluster_pkt_header), frame, frame_len);

	remote_offset = (pktio_clus->sent_pkt_count % ODP_PKTIO_MAX_PKT_COUNT) *
		ODP_PKTIO_MAX_PKT_SIZE;

	mppa_noc_dnoc_uc_set_linear_params(&uc_conf, frame_len, remote_offset);
	/* We added a local offset to our ucode */
	uc_conf.parameters[3] = (uintptr_t) tmp_pkt - (uintptr_t) &_heap_start;

	nret = mppa_noc_dnoc_uc_configure(NOC_CLUS_IFACE_ID, DNOC_CLUS_UC_ID,
					  uc_conf, header, config
#ifdef __k1a__
		, event_line
#endif
	);
	if (nret != MPPA_NOC_RET_SUCCESS)
		return 1;

	program_run.activation = 1;
	program_run.semaphore = 1;

	mppa_noc_dnoc_uc_set_program_run(NOC_CLUS_IFACE_ID, DNOC_CLUS_UC_ID,
					 program_run);

	/* FIXME asynchronous */
	mppa_noc_wait_clear_event(NOC_CLUS_IFACE_ID,
				  MPPA_NOC_INTERRUPT_LINE_DNOC_TX,
				  DNOC_CLUS_TX_ID);

	/* Modify offset for next package */
	pktio_clus->sent_pkt_count++;

	free(tmp_pkt);

	return 0;
}

static int cluster_send(pktio_entry_t *const pktio_entry,
			odp_packet_t pkt_table[], unsigned len)
{
	pkt_cluster_t *pktio_clus = &pktio_entry->s.pkt_cluster;
	odp_packet_t pkt;
	uint8_t *frame;
	uint32_t frame_len;
	unsigned i;
	int nb_tx = 0;

	for (i = 0; i < len; i++) {
		pkt = pkt_table[i];

		frame = odp_packet_l2_ptr(pkt, &frame_len);

		if(cluster_send_single_packet(pktio_clus, frame, frame_len))
			break;
	}
	nb_tx = i;

	for (i = 0; i < len; i++)
		odp_packet_free(pkt_table[i]);

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
	.open = cluster_open,
	.close = cluster_close,
	.recv = cluster_recv,
	.send = cluster_send,
	.mtu_get = cluster_mtu_get,
	.promisc_mode_set = cluster_promisc_mode_set,
	.promisc_mode_get = cluster_promisc_mode,
	.mac_get = cluster_mac_addr_get,
};
