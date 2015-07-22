/* Copyright (c) 2013, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <stdio.h>
#include <inttypes.h>
#include <HAL/hal/hal.h>

#ifndef BSP_NB_DMA_IO_MAX
#define BSP_NB_DMA_IO_MAX 1
#endif

#include "odp_rpc_internal.h"
#include <odp/plat/atomic_types.h>

#include <mppa_bsp.h>
#include <mppa_routing.h>
#include <mppa_noc.h>

static struct {
	odp_rpc_t rpc_cmd;
	uint8_t payload[RPC_MAX_PAYLOAD];
} odp_rpc_ack_buf;
static unsigned rx_port = -1;

int odp_rpc_client_init(void){
	/* Already initialized */
	if(rx_port < (unsigned)(-1))
		return 0;

	mppa_noc_ret_t ret;
	mppa_noc_dnoc_rx_configuration_t conf = {0};

	ret = mppa_noc_dnoc_rx_alloc_auto(0, &rx_port, MPPA_NOC_BLOCKING);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return -1;

	conf.buffer_base = (uintptr_t)&odp_rpc_ack_buf;
	conf.buffer_size = sizeof(odp_rpc_ack_buf),
	conf.activation = MPPA_NOC_ACTIVATED;
	conf.reload_mode = MPPA_NOC_RX_RELOAD_MODE_INCR_DATA_NOTIF;

	ret = mppa_noc_dnoc_rx_configure(0, rx_port, conf);

	return 0;
}
int odp_rpc_client_term(void){
	mppa_noc_dnoc_rx_free(0, rx_port);

	return 0;
}
void odp_rpc_print_msg(const odp_rpc_t * cmd)
{
	printf("RPC CMD:\n"
	       "\tType: %u\n"
	       "\tData: %u\n"
	       "\tDMA : %u\n"
	       "\tTag : %u\n"
	       "\tFlag: %x\n",
	       cmd->pkt_type, cmd->data_len, cmd->dma_id,
	       cmd->dnoc_tag, cmd->flags);
}

int odp_rpc_send_msg(uint16_t local_interface, uint16_t dest_id,
		     uint16_t dest_tag, odp_rpc_t * cmd,
		     void * payload)
{
	mppa_noc_ret_t ret;
	mppa_routing_ret_t rret;
	unsigned tx_port;
	mppa_dnoc_channel_config_t config;
	mppa_dnoc_header_t header;

	ret = mppa_noc_dnoc_tx_alloc_auto(local_interface,
					  &tx_port, MPPA_NOC_BLOCKING);
	if (ret != MPPA_NOC_RET_SUCCESS)
		return 1;

	/* Get and configure route */
#ifdef __k1a__
	config.word = 0;
	config._.bandwidth = mppa_noc_dnoc_get_window_length(local_interface);
#else
	config._.cfg_pe_en = 1;
	config._.cfg_user_en = 1;
	config._.write_pe_en = 1;
	config._.write_user_en = 1;
	config._.bw_current_credit = 0xff;
	config._.bw_max_credit     = 0xff;
	config._.bw_fast_delay     = 0x00;
	config._.bw_slow_delay     = 0x00;
	config._.payload_max = 32;
	config._.payload_min = 0;
#endif

	header._.tag = dest_tag;

	rret = mppa_routing_get_dnoc_unicast_route(__k1_get_cluster_id() +
						   local_interface,
						   dest_id, &config, &header);
	if (rret != MPPA_ROUTING_RET_SUCCESS)
		goto err_tx;

	ret = mppa_noc_dnoc_tx_configure(local_interface, tx_port,
					 header, config);
	if (ret != MPPA_NOC_RET_SUCCESS)
		goto err_tx;

#ifdef __k1a__
	mppa_dnoc_address_t addr =
		{ ._ = { .offset = 0, .protocol = 1, .valid = 1 }};
	mppa_noc_dnoc_tx_set_address(local_interface, tx_port, addr);
#else
	mppa_dnoc_push_offset_t off =
		{ ._ = { .offset = 0, .protocol = 1, .valid = 1 }};
	mppa_noc_dnoc_tx_set_push_offset(local_interface, tx_port, off);
#endif

	if (cmd->data_len) {
		mppa_noc_dnoc_tx_send_data(local_interface, tx_port,
					   sizeof(*cmd), cmd);
		mppa_noc_dnoc_tx_send_data_eot(local_interface, tx_port,
					   cmd->data_len, payload);
	} else {
		mppa_noc_dnoc_tx_send_data_eot(local_interface, tx_port,
					       sizeof(*cmd), cmd);
	}

	return 0;

 err_tx:
	mppa_noc_dnoc_rx_free(local_interface, tx_port);
	return 1;
}

int odp_rpc_do_query(uint16_t dest_id,
		     uint16_t dest_tag, odp_rpc_t * cmd,
		     void * payload)
{
	cmd->dma_id = __k1_get_cluster_id();
	cmd->dnoc_tag = rx_port;
	return odp_rpc_send_msg(0, dest_id, dest_tag, cmd, payload);
}

int odp_rpc_wait_ack(odp_rpc_t * cmd, void ** payload)
{
	while (!mppa_noc_dnoc_rx_lac_event_counter(0, rx_port))
		__k1_cpu_backoff(100);

	INVALIDATE(&odp_rpc_ack_buf.rpc_cmd);
	*cmd = odp_rpc_ack_buf.rpc_cmd;

	if (payload && cmd->data_len) {
		INVALIDATE_AREA(&odp_rpc_ack_buf.payload, cmd->data_len);
		*payload = odp_rpc_ack_buf.payload;
	}

	return 0;

}

