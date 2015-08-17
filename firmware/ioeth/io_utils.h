#ifndef IO_UTILS_H
#define IO_UTILS_H
#ifdef __k1io__
#include "utils.h"
#include "bsp_i2c.h"
#include "bsp_phy.h"
#include <HAL/hal/hal.h>
#include "mdio_utils.h"
#include "mdio_88E1111.h"

#define MAX_TABLE_IDX		 0	/*ONly true on fpga */
#define DEFAULT_TABLE_IDX 	 8
enum loopback_level {MAC_LOCAL_LOOPBACK= 0, MAC_BYPASS_LOOPBACK, LAST_LOOPBACK_LEVEL};

/**
 * @brief Conversion from macmode value to phy compatible mode value
 * @param mac_mode the mac_mode to convert
 * @return the equivalent phy_mode
 */
uint8_t mac_mode_to_phy_mode(enum mppa_eth_mac_ethernet_mode_e mac_mode)
{
	uint8_t phy_mode;
	switch (mac_mode) {
	case MPPABETHMAC_ETHMODE_40G:
		phy_mode = MPPA_ETH_PCIE_PHY_MODE_ETH_40G;
		break;
	case MPPABETHMAC_ETHMODE_10G_BASE_R:
		phy_mode = MPPA_ETH_PCIE_PHY_MODE_ETH_10G_BASE_R;
		break;

	case MPPABETHMAC_ETHMODE_10G_XAUI:
		phy_mode = MPPA_ETH_PCIE_PHY_MODE_ETH_10G_XAUI;
		break;

	case MPPABETHMAC_ETHMODE_10G_RXAUI:
		phy_mode = MPPA_ETH_PCIE_PHY_MODE_ETH_10G_RXAUI;
		break;

	case MPPABETHMAC_ETHMODE_1G:
		phy_mode = MPPA_ETH_PCIE_PHY_MODE_ETH_1G;
		break;

	default:
		printf("This mac mode doesn't exists !");
		exit(-1);
	}
	return phy_mode;
}


/**
 * @brief Setup the specified lane (phy must be corectly initialised before)
 * @param lane_id The lane to start
 */
static inline void start_lane(unsigned int lane_id)
{
	/* Reset SERDES */
	mppabeth_phy_disable_clk((void *) mppa_eth_pcie_csr[0], lane_id);
	__builtin_k1_fence();
	/* Reset MAC */
	mppabeth_mac_enable_reset((void *) &mppa_ethernet[0]->mac);
	__builtin_k1_fence();


	mppabeth_phy_power_on_lane((void *) mppa_eth_pcie_csr[0], lane_id);

	mppabeth_phy_cfg_rx_pstate((void *) mppa_eth_pcie_csr[0], 0, 0);
	mppabeth_phy_cfg_tx_pstate((void *) mppa_eth_pcie_csr[0], 0, 0);
	mppabeth_phy_cfg_rx_pstate((void *) mppa_eth_pcie_csr[0], 1, 0);
	mppabeth_phy_cfg_tx_pstate((void *) mppa_eth_pcie_csr[0], 1, 0);
	mppabeth_phy_cfg_rx_pstate((void *) mppa_eth_pcie_csr[0], 2, 0);
	mppabeth_phy_cfg_tx_pstate((void *) mppa_eth_pcie_csr[0], 2, 0);
	mppabeth_phy_cfg_rx_pstate((void *) mppa_eth_pcie_csr[0], 3, 0);
	mppabeth_phy_cfg_tx_pstate((void *) mppa_eth_pcie_csr[0], 3, 0);
	mppa_cycle_delay(2000);

	/* MAC update */
	mppabeth_mac_enable_lane((void *) &(mppa_ethernet[0]->mac), lane_id);
	mppabeth_mac_disable_reset((void *) &(mppa_ethernet[0]->mac));
	mppa_cycle_delay(100);


	/* Set up SERDES */
	mppabeth_phy_enable_clk((void *) mppa_eth_pcie_csr[0], lane_id);

	unsigned int i = 0;
	for (i = 0; i < 4; i++) {
		__k1_phy_lane_reset(i);
	}
}


volatile mppa_i2c_master_t *ethernet_i2c_master = NULL;


int mppa_eth_i2c_read(uint8_t chip_id, void *uarg, unsigned reg, uint16_t * pval)
{
	UNUSED(uarg);
	return mppa_i2c_register_read((void *) ethernet_i2c_master, chip_id, reg, pval);
}

int mppa_eth_i2c_write(uint8_t chip_id, void *uarg, unsigned reg, uint16_t val)
{
	UNUSED(uarg);
	return mppa_i2c_register_write((void *) ethernet_i2c_master, chip_id, reg, val);
}

void dump_reg_dev(uint8_t dev)
{
	int status = 0;
	uint16_t val;
	uint8_t i;

	for (i = 0; i < 32; i++) {
		//      mppa_eth_i2c_write(dev, NULL, 22, 0x0);
		status |= mppa_eth_i2c_read(dev, NULL, i, &val);
		if (status != 0) {
			printf("[88E1111 0x] 88E1111 register dump failed.\n");
		}
		if (val != 0xffff)
			printf("[88E1111 0x] Register %d: 0x%.4x\n", i, val);
		//              mppa_cycle_delay(100000);
	}
}


/**
 * @brief Complete mac initialization => 
 * - synchro with the PHY 
 * - startup the lane
 * - Check the lane status 
 * @return 
 */
static mppa_88E1111_interface_t i2c_ifce;

uint32_t init_mac(const int lane_id)
{
	__k1_streaming_disable();
	if (__k1_phy_init_full(MPPA_ETH_PCIE_PHY_MODE_ETH_1G, 1, MPPA_PERIPH_CLOCK_156) != 0x01) {
		printf("Reset PHY failed\n");
		return -1;
	}

	if (__bsp_flavour == BSP_ETH_530 || __bsp_flavour == BSP_EXPLORER) {
		mppa_eth_mdio_synchronize();
	}

	else if (__bsp_flavour == BSP_DEVELOPER) {
		ethernet_i2c_master = setup_i2c_master(0, 1, I2C_BITRATE, GPIO_RATE);
		i2c_ifce.i2c_master = ethernet_i2c_master;
		i2c_ifce.i2c_bus = 2;
		i2c_ifce.i2c_coma_pin = 15;
		i2c_ifce.i2c_reset_n_pin = 16;
		i2c_ifce.i2c_int_n_pin = 14;
		i2c_ifce.i2c_gic = 0;
		i2c_ifce.chip_id = 0x41;

		mppa_i2c_init(i2c_ifce.i2c_master,
			      i2c_ifce.i2c_bus,
			      i2c_ifce.i2c_coma_pin,
			      i2c_ifce.i2c_reset_n_pin, i2c_ifce.i2c_int_n_pin, i2c_ifce.i2c_gic);
		i2c_ifce.context = i2c_ifce.i2c_master;
		i2c_ifce.mppa_88E1111_read = mppa_i2c_register_read;
		i2c_ifce.mppa_88E1111_write = mppa_i2c_register_write;

		mppa_88E1111_configure(&i2c_ifce);
		mppa_88E1111_synchronize(&i2c_ifce);
	} else {
		printf("This platform is not suppotred yet !\n");
		exit(-1);
	}

	mppabeth_mac_cfg_mode((void *) &(mppa_ethernet[0]->mac), MPPABETHMAC_ETHMODE_1G);
	mppabeth_mac_cfg_sgmii_rate((void *) &(mppa_ethernet[0]->mac), MPPABETHMAC_SGMIIRATE_1G);
	enum mppa_eth_mac_1G_mode_e sgmii_rate __attribute__ ((unused));
	sgmii_rate = mppabeth_mac_get_sgmii_rate((void *) &(mppa_ethernet[0]->mac));
	//Basic mac settings
	mppabeth_mac_enable_rx_check_sfd((void *)
					 &(mppa_ethernet[0]->mac));
	mppabeth_mac_enable_rx_check_preambule((void *)
					       &(mppa_ethernet[0]->mac));
	mppabeth_mac_enable_rx_fcs_deletion((void *)
					    &(mppa_ethernet[0]->mac));
	mppabeth_mac_enable_tx_fcs_insertion((void *)
					     &(mppa_ethernet[0]->mac));
	mppabeth_mac_enable_tx_add_padding((void *)
					   &(mppa_ethernet[0]->mac));
	/* printf("RESET = %x\n", __k1_ethernet_get_ethernet_reset_status()); */

	/* printf("Avant de valid les lanes\n");         */
	start_lane(lane_id);

	/* printf ("Polling for link %d\n", lane_id); */
	while (((mppabeth_mac_get_stat_rx_status((void *) &(mppa_ethernet[0]->mac), lane_id) != 1)
		||
		(mppabeth_mac_get_stat_rx_block_lock
		 ((void *) &(mppa_ethernet[0]->mac), lane_id) != 1)));
	printf("Link %d up\n", lane_id);
	return 0;
}

void set_no_fcs()
{
	mppabeth_mac_disable_rx_fcs_deletion((void *)
					     &(mppa_ethernet[0]->mac));
	mppabeth_mac_disable_tx_fcs_insertion((void *)
					      &(mppa_ethernet[0]->mac));
}

void set_fcs()
{
	mppabeth_mac_enable_rx_fcs_deletion((void *)
					    &(mppa_ethernet[0]->mac));
	mppabeth_mac_enable_tx_fcs_insertion((void *)
					     &(mppa_ethernet[0]->mac));
}




void configure_dnoc_ethernet_lb(uint32_t ethernet_dnoc_interface,
				uint32_t dnoc_interface,
				uint32_t tx_id_for_mac,
				uint64_t first_dir_for_rx_packets,
				uint64_t route_for_rx_packets,
				uint16_t context,
				uint64_t min_tag_for_rx_packets, uint64_t max_tag_for_rx_packets)
{
	/*ETH RX part */
	// ETH => RM TX
	mppabeth_lb_cfg_default_dispatch_policy((void *)
						&(mppa_ethernet[0]->lb),
						ethernet_dnoc_interface,
						MPPABETHLB_DISPATCH_DEFAULT_POLICY_RR);
	mppabeth_lb_cfg_global_mode((void *) &(mppa_ethernet[0]->lb),
				    ethernet_dnoc_interface,
				    MPPABETHLB_STORE_AND_FORWARD,
				    MPPABETHLB_KEEP_PKT_CRC_ERROR,
				    MPPABETHLB_ADD_HEADER,
				    MPPABETHLB_NO_FOOTER, MPPABETHLB_JUMBO_DISABLED);
	mppabeth_lb_cfg_table_rr_dispatch_channel((void *)
						  &(mppa_ethernet[0]->lb),
						  DEFAULT_TABLE_IDX,
						  ethernet_dnoc_interface, dnoc_interface - 4
						  /* should be dnoc_interface but numbered from 4 :   */
						  , tx_id_for_mac, 1 << context);

	/* Open a  TX to packet RX */
	// RM TX => RM RX
	mppa_dnoc_header_t header;
	header._.route = route_for_rx_packets;
	header._.valid = 1;
	header._.tag = min_tag_for_rx_packets;
	__k1_dnoc_configure_tx(dnoc_interface, tx_id_for_mac,
			       first_dir_for_rx_packets, header.dword, 0);
	mppa_dnoc[dnoc_interface]->tx_chan_route[tx_id_for_mac].min_max_task_id[context]._.
	    current_task_id = min_tag_for_rx_packets;
	mppa_dnoc[dnoc_interface]->tx_chan_route[tx_id_for_mac].min_max_task_id[context]._.
	    min_task_id = min_tag_for_rx_packets;
	mppa_dnoc[dnoc_interface]->tx_chan_route[tx_id_for_mac].min_max_task_id[context]._.
	    max_task_id = max_tag_for_rx_packets;
	mppa_dnoc[dnoc_interface]->tx_chan_route[tx_id_for_mac].min_max_task_id[context]._.
	    min_max_task_id_en = 1;
}

void configure_dnoc_ethernet_tx(uint32_t dnoc_interface,
				uint32_t rx_tag_to_mac, uint32_t lane_id, uint32_t fifo_id)
{
	/*ETH TX part */
	/* configure dma rx to ETH TX */

	__k1_dnoc_configure_rx_ext(dnoc_interface, rx_tag_to_mac,
				   _K1_NOCV2_INCR_DATA_NOTIF,
				   (void
				    *) (&(mppa_ethernet[0]->tx.fifo_if[0].lane[lane_id].
					  eth_fifo[fifo_id].push_data)), 8, 0, 0);
	mppa_dnoc[dnoc_interface]->rx_queues[rx_tag_to_mac].event_it_target.word = 0x0;
	__k1_dnoc_activate_rx_mode_ext(dnoc_interface, rx_tag_to_mac, 5);
}

void dump_stat(mppa_ethernet_lane_stat_t * stat)
{
	printf(""
	       "stat->total_rx_bytes_nb = %llu\n"
	       "stat->good_rx_bytes_nb, = %llu\n"
	       "stat->total_rx_packet_nb, = %u\n"
	       "stat->good_rx_packet_nb, = %u\n"
	       "stat->rx_broadcast, = %u\n"
	       "stat->rx_multicast, = %u\n"
	       "stat->rx_packet_64_bytes, = %u\n"
	       "stat->rx_packet_65_127_bytes, = %u\n"
	       "stat->rx_packet_128_255_bytes, = %u\n"
	       "stat->rx_packet_256_511_bytes, = %u\n"
	       "stat->rx_packet_512_1023_bytes, = %u\n"
	       "stat->rx_packet_1024_1522_bytes, = %u\n"
	       "stat->rx_packet_1523_9215_bytes = %u\n"
	       "stat->rx_vlan = %u\n"
	       "stat->total_tx_bytes_nb = %llu\n"
	       "stat->total_tx_packet_nb, = %u\n"
	       "stat->tx_broadcast, = %u\n"
	       "stat->tx_multicast, = %u\n"
	       "stat->tx_packet_64_bytes, = %u\n"
	       "stat->tx_packet_65_127_bytes, = %u\n"
	       "stat->tx_packet_128_255_bytes, = %u\n"
	       "stat->tx_packet_256_511_bytes, = %u\n"
	       "stat->tx_packet_512_1023_bytes, = %u\n"
	       "stat->tx_packet_1024_1522_bytes, = %u\n"
	       "stat->tx_packet_1523_9215_bytes = %u\n"
	       "stat->tx_vlan = %u\n",
	       stat->total_rx_bytes_nb.reg,
	       stat->good_rx_bytes_nb.reg,
	       stat->total_rx_packet_nb.reg,
	       stat->good_rx_packet_nb.reg,
	       stat->rx_broadcast.reg,
	       stat->rx_multicast.reg,
	       stat->rx_packet_64_bytes.reg,
	       stat->rx_packet_65_127_bytes.reg,
	       stat->rx_packet_128_255_bytes.reg,
	       stat->rx_packet_256_511_bytes.reg,
	       stat->rx_packet_512_1023_bytes.reg,
	       stat->rx_packet_1024_1522_bytes.reg,
	       stat->rx_packet_1523_9215_bytes.reg,
	       stat->rx_vlan.reg,
	       stat->total_tx_bytes_nb.reg,
	       stat->total_tx_packet_nb.reg,
	       stat->tx_broadcast.reg,
	       stat->tx_multicast.reg,
	       stat->tx_packet_64_bytes.reg,
	       stat->tx_packet_65_127_bytes.reg,
	       stat->tx_packet_128_255_bytes.reg,
	       stat->tx_packet_256_511_bytes.reg,
	       stat->tx_packet_512_1023_bytes.reg,
	       stat->tx_packet_1024_1522_bytes.reg,
	       stat->tx_packet_1523_9215_bytes.reg, stat->tx_vlan.reg);
}

void copy_stat(unsigned int lane_id, mppa_ethernet_lane_stat_t * out)
{
	out->total_rx_bytes_nb.reg = mppabeth_mac_get_total_rx_bytes_nb((void *)
									&(mppa_ethernet[0]->mac),
									lane_id);
	out->total_rx_packet_nb.reg = mppabeth_mac_get_total_rx_packet_nb((void *)
									  &(mppa_ethernet[0]->mac),
									  lane_id);
	out->good_rx_bytes_nb.reg = mppabeth_mac_get_good_rx_bytes_nb((void *)
								      &(mppa_ethernet[0]->mac),
								      lane_id);
	out->good_rx_packet_nb.reg = mppabeth_mac_get_good_rx_packet_nb((void *)
									&(mppa_ethernet[0]->mac),
									lane_id);
	out->rx_broadcast.reg = mppabeth_mac_get_rx_broadcast((void *)
							      &(mppa_ethernet[0]->mac), lane_id);
	out->rx_multicast.reg = mppabeth_mac_get_rx_multicast((void *)
							      &(mppa_ethernet[0]->mac), lane_id);
	out->rx_packet_64_bytes.reg = mppabeth_mac_get_rx_packet_64_bytes((void *)
									  &(mppa_ethernet[0]->mac),
									  lane_id);
	out->rx_packet_65_127_bytes.reg = mppabeth_mac_get_rx_packet_65_127_bytes((void *)
										  &(mppa_ethernet
										    [0]->mac),
										  lane_id);
	out->rx_packet_128_255_bytes.reg = mppabeth_mac_get_rx_packet_128_255_bytes((void *)
										    &(mppa_ethernet
										      [0]->mac),
										    lane_id);
	out->rx_packet_256_511_bytes.reg = mppabeth_mac_get_rx_packet_256_511_bytes((void *)
										    &(mppa_ethernet
										      [0]->mac),
										    lane_id);
	out->rx_packet_512_1023_bytes.reg = mppabeth_mac_get_rx_packet_512_1023_bytes((void *)
										      &
										      (mppa_ethernet
										       [0]->mac),
										      lane_id);
	out->rx_packet_1024_1522_bytes.reg = mppabeth_mac_get_rx_packet_1024_1522_bytes((void *)
											&
											(mppa_ethernet
											 [0]->mac),
											lane_id);
	out->rx_packet_1523_9215_bytes.reg = mppabeth_mac_get_rx_packet_1523_9215_bytes((void *)
											&
											(mppa_ethernet
											 [0]->mac),
											lane_id);
	out->rx_vlan.reg = mppabeth_mac_get_rx_vlan((void *) &(mppa_ethernet[0]->mac), lane_id);
	out->total_tx_bytes_nb.reg = mppabeth_mac_get_total_tx_bytes_nb((void *)
									&(mppa_ethernet[0]->mac),
									lane_id);
	out->total_tx_packet_nb.reg = mppabeth_mac_get_total_tx_packet_nb((void *)
									  &(mppa_ethernet[0]->mac),
									  lane_id);
	out->tx_broadcast.reg = mppabeth_mac_get_tx_broadcast((void *)
							      &(mppa_ethernet[0]->mac), lane_id);
	out->tx_multicast.reg = mppabeth_mac_get_tx_multicast((void *)
							      &(mppa_ethernet[0]->mac), lane_id);
	out->tx_packet_64_bytes.reg = mppabeth_mac_get_tx_packet_64_bytes((void *)
									  &(mppa_ethernet[0]->mac),
									  lane_id);
	out->tx_packet_65_127_bytes.reg = mppabeth_mac_get_tx_packet_65_127_bytes((void *)
										  &(mppa_ethernet
										    [0]->mac),
										  lane_id);
	out->tx_packet_128_255_bytes.reg = mppabeth_mac_get_tx_packet_128_255_bytes((void *)
										    &(mppa_ethernet
										      [0]->mac),
										    lane_id);
	out->tx_packet_256_511_bytes.reg = mppabeth_mac_get_tx_packet_256_511_bytes((void *)
										    &(mppa_ethernet
										      [0]->mac),
										    lane_id);
	out->tx_packet_512_1023_bytes.reg = mppabeth_mac_get_tx_packet_512_1023_bytes((void *)
										      &
										      (mppa_ethernet
										       [0]->mac),
										      lane_id);
	out->tx_packet_1024_1522_bytes.reg = mppabeth_mac_get_tx_packet_1024_1522_bytes((void *)
											&
											(mppa_ethernet
											 [0]->mac),
											lane_id);
	out->tx_packet_1523_9215_bytes.reg = mppabeth_mac_get_tx_packet_1523_9215_bytes((void *)
											&
											(mppa_ethernet
											 [0]->mac),
											lane_id);
	out->tx_vlan.reg = mppabeth_mac_get_tx_vlan((void *) &(mppa_ethernet[0]->mac), lane_id);
}

int compare_stat(mppa_ethernet_lane_stat_t * a, mppa_ethernet_lane_stat_t * b)
{
	return (a->total_rx_packet_nb.reg == b->total_rx_packet_nb.reg &&
		a->total_rx_bytes_nb.reg == b->total_rx_bytes_nb.reg &&
		a->good_rx_bytes_nb.reg == b->good_rx_bytes_nb.reg &&
		a->good_rx_packet_nb.reg == b->good_rx_packet_nb.reg &&
		a->rx_broadcast.reg == b->rx_broadcast.reg &&
		a->rx_multicast.reg == b->rx_multicast.reg &&
		a->rx_packet_64_bytes.reg == b->rx_packet_64_bytes.reg &&
		a->rx_packet_65_127_bytes.reg ==
		b->rx_packet_65_127_bytes.reg
		&& a->rx_packet_128_255_bytes.reg ==
		b->rx_packet_128_255_bytes.reg
		&& a->rx_packet_256_511_bytes.reg ==
		b->rx_packet_256_511_bytes.reg
		&& a->rx_packet_512_1023_bytes.reg ==
		b->rx_packet_512_1023_bytes.reg
		&& a->rx_packet_1024_1522_bytes.reg ==
		b->rx_packet_1024_1522_bytes.reg
		&& a->rx_packet_1523_9215_bytes.reg ==
		b->rx_packet_1523_9215_bytes.reg
		&& a->rx_vlan.reg == b->rx_vlan.reg
		&& a->total_tx_packet_nb.reg == b->total_tx_packet_nb.reg
		&& a->total_tx_bytes_nb.reg == b->total_tx_bytes_nb.reg
		&& a->tx_broadcast.reg == b->tx_broadcast.reg
		&& a->tx_multicast.reg == b->tx_multicast.reg
		&& a->tx_packet_64_bytes.reg == b->tx_packet_64_bytes.reg
		&& a->tx_packet_65_127_bytes.reg ==
		b->tx_packet_65_127_bytes.reg
		&& a->tx_packet_128_255_bytes.reg ==
		b->tx_packet_128_255_bytes.reg
		&& a->tx_packet_256_511_bytes.reg ==
		b->tx_packet_256_511_bytes.reg
		&& a->tx_packet_512_1023_bytes.reg ==
		b->tx_packet_512_1023_bytes.reg
		&& a->tx_packet_1024_1522_bytes.reg ==
		b->tx_packet_1024_1522_bytes.reg
		&& a->tx_packet_1523_9215_bytes.reg ==
		b->tx_packet_1523_9215_bytes.reg && a->tx_vlan.reg == b->tx_vlan.reg);
}



#endif
#endif				/* IO_UTILS_H */
