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

#define CNOC_CLUS_TX_ID		0
#define CNOC_CLUS_BASE_RX_ID	0

#define NOC_CLUS_IFACE_ID	0

#define ODP_PKTIO_MAX_PKT_SIZE		(9 * 1024)
#define ODP_PKTIO_MAX_PKT_COUNT		5
#define ODP_PKTIO_PKT_BUF_SIZE		(ODP_PKTIO_MAX_PKT_COUNT * ODP_PKTIO_MAX_PKT_SIZE)

int cluster_global_init(void);
void cluster_mac_get(const pktio_entry_t *const pktio_entry, void * mac_addr);
int cluster_init_entry(pktio_entry_t * pktio_entry, odp_pool_t pool);
int cluster_open(pktio_entry_t * pktio_entry, const char *dev);
int cluster_recv(pktio_entry_t *const entry, odp_packet_t pkt_table[], int len);
int cluster_send(pktio_entry_t *const entry, odp_packet_t pkt_table[], unsigned len);
int cluster_promisc_mode_set(pktio_entry_t *const pktio_entry, odp_bool_t enable);
int cluster_promisc_mode(pktio_entry_t *const pktio_entry);
int cluster_mtu_get(pktio_entry_t *const pktio_entry);

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
				g_available_clus[g_available_clus_count++] = __bsp_platform[i].node_id;
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
	ret = mppa_noc_dnoc_rx_alloc(NOC_CLUS_IFACE_ID,  DNOC_CLUS_BASE_RX + clus_id);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	conf.buffer_base = (uintptr_t) g_pkt_recv_buf[clus_id];
	conf.buffer_size = ODP_PKTIO_PKT_BUF_SIZE;
	conf.activation = MPPA_NOC_ACTIVATED;
	conf.reload_mode = MPPA_NOC_RX_RELOAD_MODE_INCR_DATA_NOTIF;

	ret = mppa_noc_dnoc_rx_configure(NOC_CLUS_IFACE_ID,  DNOC_CLUS_BASE_RX + clus_id, conf);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	return 0;
}

static int cluster_init_cnoc_rx(int clus_id)
{
	mppa_noc_ret_t ret;
	mppa_noc_cnoc_rx_configuration_t conf = {0};

	conf.mode = MPPA_NOC_CNOC_RX_MAILBOX;
	conf.init_value = 0;

	/* CNoC */
	ret = mppa_noc_cnoc_rx_alloc(NOC_CLUS_IFACE_ID,  CNOC_CLUS_BASE_RX_ID + clus_id);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	ret = mppa_noc_cnoc_rx_configure(NOC_CLUS_IFACE_ID, CNOC_CLUS_BASE_RX_ID + clus_id, conf);
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

	return 0;
}

static int cluster_init_noc_tx(void)
{
	mppa_noc_ret_t ret;
	mppa_noc_dnoc_uc_configuration_t uc_conf;

	uc_conf.program_start = (uintptr_t) odp_ucode_linear;
	uc_conf.buffer_base = 0x0;
	uc_conf.buffer_size = 2 * 1024 * 1024;

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

	ret = mppa_noc_dnoc_uc_link(NOC_CLUS_IFACE_ID, DNOC_CLUS_UC_ID, DNOC_CLUS_TX_ID, uc_conf);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	/* CnoC */
	ret = mppa_noc_cnoc_tx_alloc(NOC_CLUS_IFACE_ID, CNOC_CLUS_TX_ID);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	return 0;
}

int cluster_global_init(void)
{
	mppacl_init_available_clusters();

	if (cluster_init_noc_rx())
		return 1;

	if (cluster_init_noc_tx())
		return 1;

	sleep(1);

	return 0;
}


int cluster_init_entry(pktio_entry_t * pktio_entry, odp_pool_t pool)
{
	pktio_cluster_t * pkt_cluster = &pktio_entry->s.cluster;

	pkt_cluster->pool = pool;
	pkt_cluster->sent_pkt_count = 0;
	pkt_cluster->recv_pkt_count = 0;

	/* pkt buffer size */
	pkt_cluster->buf_size = odp_buffer_pool_segment_size(pool);
	/* max frame len taking into account the l2-offset */
	pkt_cluster->max_frame_len = pkt_cluster->buf_size -
		odp_buffer_pool_headroom(pool) -
		odp_buffer_pool_tailroom(pool);
	return 0;
}

int cluster_open(pktio_entry_t * pktio_entry, const char *dev)
{
	pktio_cluster_t *pktio_clus = &pktio_entry->s.cluster;
	if(!strncmp("cluster-", dev, strlen("cluster-"))) {
		/* String should in the following format: "cluster-<cluster_id>" */
		pktio_entry->s.type = ODP_PKTIO_TYPE_CLUSTER;
		pktio_clus->clus_id = atoi(dev+strlen("cluster-"));

		if (pktio_clus->clus_id < 0 || pktio_clus->clus_id > 15) {
			ODP_ERR("Invalid cluster id '%d'", pktio_clus->clus_id);
			return 1;
		}
		printf("Opening cluster %d\n", pktio_clus->clus_id);
		return 0;
	}

	return -1;
}

void cluster_mac_get(const pktio_entry_t *const pktio_entry, void * mac_addr)
{
	const pktio_cluster_t *pktio_clus = &pktio_entry->s.cluster;
	uint8_t *mac_addr_u = mac_addr;

	memset(mac_addr_u, 0, ETH_ALEN);

	mac_addr_u[0] = pktio_clus->clus_id;
}

//~ static int cluster_receive_single_packet(pktio_cluster_t *pktio_clus, odp_packet_t *pkt)
//~ {
	//~ odp_packet_t pkt = ODP_PACKET_INVALID;
	//~ uint8_t *pkt_buf;
	//~ ssize_t recv_bytes;
//~ 
	//~ pkt = odp_packet_alloc(pktio_entry->s.cluster.pool, pktio_entry->s.cluster.max_frame_len);
	//~ if (odp_unlikely(pkt == ODP_PACKET_INVALID))
		//~ return 1;
//~ 
	//~ pkt_buf = odp_packet_data(pkt);
//~ 
	//~ recv_bytes = _magic_scall_recv(pktio_entry->s.magic.fd, pkt_buf);
//~ 
	//~ /* no data or error: free recv buf and break out of loop */
	//~ if (odp_unlikely(recv_bytes < 1))
		//~ break;
	//~ /* /\* frame not explicitly for us, reuse pkt buf for next frame *\/ */
	//~ /* if (odp_unlikely(sll.sll_pkttype == PACKET_OUTGOING)) */
	//~ /* 	continue; */
//~ 
	//~ /* Parse and set packet header data */
	//~ odp_packet_pull_tail(pkt, pktio_entry->s.magic.max_frame_len - recv_bytes);
	//~ _odp_packet_reset_parse(pkt);
//~ 
	//~ pkt_table[nb_rx] = pkt;
//~ 
	//~ nb_rx++;
	//~ return 0;
//~ }

int cluster_recv(__attribute__((unused)) pktio_entry_t *const pktio_entry, __attribute__((unused))odp_packet_t pkt_table[], __attribute__((unused))int len)
{
	//~ mppa_noc_dnoc_rx_counters_t counter;
	//~ pktio_cluster_t *pktio_clus = &pktio_entry->s.cluster;
	//~ unsigned int nb_tx, i;
//~ 
	//~ /* Check if we received some packets */
	//~ counter = mppa_noc_dnoc_rx_lac_item_event_counter(NOC_CLUS_IFACE_ID, DNOC_CLUS_BASE_RX + pktio_clus->clus_id);
	//~ if (counter.event_counter == 0)
		//~ return 0;
//~ 
	//~ for (nb_tx = 0; nb_tx < counter.event_counter) {
		//~ cluster_receive_single_packet(pkt_clus, opkt_tabe
	//~ }

	return 0;
}

static inline int
cluster_send_single_packet(pktio_cluster_t *pktio_clus,  __attribute__((unused)) void *frame, uint32_t frame_len)
{
	mppa_noc_dnoc_uc_configuration_t uc_conf;
	mppa_dnoc_channel_config_t config = { 0 };
	mppa_dnoc_header_t header = { 0 };
	mppa_noc_event_line_t event_line;
	mppa_noc_uc_program_run_t program_run;
	mppa_noc_ret_t nret;
	mppa_routing_ret_t rret;
	uint64_t remote_pkt_count;

	uc_conf.pointers = NULL;
	uc_conf.event_counter = 0;

	/* Get credits first */
	remote_pkt_count = mppa_noc_cnoc_rx_get_value(NOC_CLUS_IFACE_ID, CNOC_CLUS_BASE_RX_ID + pktio_clus->clus_id);

	/* Is there enough room to send a packet ? */
	if ((pktio_clus->sent_pkt_count - remote_pkt_count) >= ODP_PKTIO_MAX_PKT_COUNT)
		return 1;

	/* Wait for the ucode event */
	if (g_uc_is_running)

	/* Event config */
	event_line.line = MPPA_NOC_USE_EVENT;
	event_line.pe_mask = __k1_get_cpu_id();

	mppa_noc_dnoc_uc_set_linear_params(&uc_conf, frame_len, pktio_clus->sent_pkt_count % ODP_PKTIO_MAX_PKT_COUNT);

	/* Get and configure route */
	config._.bandwidth = mppa_noc_dnoc_get_window_length(NOC_CLUS_IFACE_ID);

	header._.tag = DNOC_CLUS_BASE_RX + __k1_get_cluster_id();
	
	rret = mppa_routing_get_dnoc_unicast_route(__k1_get_cluster_id(), pktio_clus->clus_id, &config, &header);
	if (rret != MPPA_ROUTING_RET_SUCCESS)
		return 1;

	nret = mppa_noc_dnoc_uc_configure(NOC_CLUS_IFACE_ID, DNOC_CLUS_UC_ID, uc_conf, header, config, event_line);
	if (nret != MPPA_NOC_RET_SUCCESS)
		return 1;

	program_run.activation = 1;
	program_run.semaphore = 1;

	mppa_noc_dnoc_uc_set_program_run(NOC_CLUS_IFACE_ID, DNOC_CLUS_UC_ID, program_run);

	/* FIXME asynchronous */
	mppa_noc_wait_clear_event(NOC_CLUS_IFACE_ID, MPPA_NOC_INTERRUPT_LINE_DNOC_TX, DNOC_CLUS_TX_ID);

	/* Modify offset for next package */
	pktio_clus->sent_pkt_count++;
	

	return 0;
}

int cluster_send(pktio_entry_t *const pktio_entry, odp_packet_t pkt_table[], unsigned len)
{
	pktio_cluster_t *pktio_clus = &pktio_entry->s.cluster;
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

int cluster_promisc_mode_set(__attribute__((unused))pktio_entry_t *const pktio_entry, __attribute__((unused))odp_bool_t enable)
{
	return 0;
}

int cluster_promisc_mode(__attribute__((unused))pktio_entry_t *const pktio_entry)
{
	return 0;
}

int cluster_mtu_get(__attribute__((unused)) pktio_entry_t *const pktio_entry)
{
	return ODP_PKTIO_MAX_PKT_SIZE;
}

struct pktio_if_operation cluster_pktio_operation = {
	.name = "cluster",
	.global_init = cluster_global_init,
	.setup_pktio_entry = cluster_init_entry,
	.mac_get = cluster_mac_get,
	.recv = cluster_recv,
	.send = cluster_send,
	.promisc_mode_set = cluster_promisc_mode_set,
	.promisc_mode_get = cluster_promisc_mode,
	.mtu_get = cluster_mtu_get,
	.open = cluster_open,
};
