#ifndef IO_UTILS_H
#define IO_UTILS_H
#ifdef __k1io__
#include "utils.h"
#include "bsp_phy.h"
#include <HAL/hal/hal.h>
#include "mdio_utils.h"
#include <libmppa_eth_mac.h>
#include <libmppa_eth_phy_core.h>
#include <libmppa_eth_loadbalancer.h>
#include <bsp_phy.h>

#define MAX_TABLE_IDX		 0	/*ONly true on fpga */
#define DEFAULT_TABLE_IDX 	 8
enum loopback_level { MAC_LOCAL_LOOPBACK =
	    0, MAC_BYPASS_LOOPBACK, LAST_LOOPBACK_LEVEL };

/**
 * @brief Conversion from macmode value to phy compatible mode value
 * @param mac_mode the mac_mode to convert
 * @return the equivalent phy_mode
 */
uint8_t mac_mode_to_phy_mode(enum mppa_eth_mac_ethernet_mode_e mac_mode)
{
	uint8_t phy_mode;
	switch (mac_mode) {
	case MPPA_ETH_MAC_ETHMODE_40G:
		phy_mode = MPPA_ETH_PCIE_PHY_MODE_ETH_40G;
		break;
	case MPPA_ETH_MAC_ETHMODE_10G_BASE_R:
		phy_mode = MPPA_ETH_PCIE_PHY_MODE_ETH_10G_BASE_R;
		break;

	case MPPA_ETH_MAC_ETHMODE_10G_XAUI:
		phy_mode = MPPA_ETH_PCIE_PHY_MODE_ETH_10G_XAUI;
		break;

	case MPPA_ETH_MAC_ETHMODE_10G_RXAUI:
		phy_mode = MPPA_ETH_PCIE_PHY_MODE_ETH_10G_RXAUI;
		break;

	case MPPA_ETH_MAC_ETHMODE_1G:
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
	/* uint32_t lane_valid = 1 << lane_id; */
	/* Reset SERDES */
	mppabeth_phy_disable_clk((void *) mppa_eth_pcie_csr[0], lane_id);
	__builtin_k1_fence();
	/* Reset MAC */
	mppabeth_mac_enable_reset((void *)&(mppa_ethernet[0]->mac));
	__builtin_k1_fence();


	/* MAC update */
	mppabeth_mac_enable_lane((void *)&(mppa_ethernet[0]->mac), lane_id);
	mppabeth_mac_disable_reset((void *)&(mppa_ethernet[0]->mac));
	__builtin_k1_fence();
	mppa_cycle_delay(100);
	/* Set up SERDES */
	mppabeth_phy_enable_clk((void *) mppa_eth_pcie_csr[0], lane_id);
	unsigned int i = 0;
	for (i = 0; i < 4; i++) {
		__k1_phy_lane_reset(i);
	}
}

/**
 * @brief Complete mac initialization => 
 * - synchro with the PHY 
 * - startup the lane
 * - Check the lane status 
 * @return 
 */
uint32_t init_mac()
{
	int lane_id = 0;
	mppa_eth_mdio_synchronize();
	//FIXME: Ugly Hack Need to build correctly as explorer not eth530
	//__bsp_flavour = BSP_EXPLORER;
	if (__k1_phy_init_full(mac_mode_to_phy_mode(MPPA_ETH_MAC_ETHMODE_1G), 1, MPPA_PERIPH_CLOCK_156) !=
	    0x01) {
		printf("Reset PHY failed\n");
		return -1;
	}
	//Set in 1G mode (only available mode on FPGA)
	mppabeth_mac_cfg_mode((void *)&(mppa_ethernet[0]->mac), MPPA_ETH_MAC_ETHMODE_1G);
	mppabeth_mac_cfg_sgmii_rate((void *)&(mppa_ethernet[0]->mac), MPPA_ETH_MAC_SGMIIRATE_1G);
	//Basic mac settings
	mppabeth_mac_enable_rx_check_sfd((void *)&(mppa_ethernet[0]->mac));
	mppabeth_mac_enable_rx_check_preambule((void *)&(mppa_ethernet[0]->mac));
	mppabeth_mac_enable_rx_fcs_deletion((void *)&(mppa_ethernet[0]->mac));
	mppabeth_mac_enable_tx_fcs_insertion((void *)&(mppa_ethernet[0]->mac));
	mppabeth_mac_enable_tx_add_padding((void *)&(mppa_ethernet[0]->mac));
	start_lane(lane_id);
	DMSG("Polling for link\n");
	while ((mppabeth_mac_get_stat_rx_status
		((void *) &(mppa_ethernet[0]->mac), lane_id) != 1)
	       ||
	       (mppabeth_mac_get_stat_rx_block_lock
		((void *) &(mppa_ethernet[0]->mac), lane_id) != 1));
	printf("Link up\n");
	return 0;
}

void set_no_fcs()
{
	mppabeth_mac_disable_rx_fcs_deletion((void *)&(mppa_ethernet[0]->mac));
	mppabeth_mac_disable_tx_fcs_insertion((void *)&(mppa_ethernet[0]->mac));
}

void set_fcs()
{
	mppabeth_mac_enable_rx_fcs_deletion((void *)&(mppa_ethernet[0]->mac));
	mppabeth_mac_enable_tx_fcs_insertion((void *)&(mppa_ethernet[0]->mac));
}


/* void dump_stat(mppa_ethernet_lane_stat_t * stat) */
/* { */
/* 	printf("" */
/* 	       "stat->total_rx_bytes_nb = %llu\n" */
/* 	       "stat->good_rx_bytes_nb, = %llu\n" */
/* 	       "stat->total_rx_packet_nb, = %u\n" */
/* 	       "stat->good_rx_packet_nb, = %u\n" */
/* 	       "stat->rx_broadcast, = %u\n" */
/* 	       "stat->rx_multicast, = %u\n" */
/* 	       "stat->rx_packet_64_bytes, = %u\n" */
/* 	       "stat->rx_packet_65_127_bytes, = %u\n" */
/* 	       "stat->rx_packet_128_255_bytes, = %u\n" */
/* 	       "stat->rx_packet_256_511_bytes, = %u\n" */
/* 	       "stat->rx_packet_512_1023_bytes, = %u\n" */
/* 	       "stat->rx_packet_1024_1522_bytes, = %u\n" */
/* 	       "stat->rx_packet_1523_9215_bytes = %u\n" */
/* 	       "stat->rx_vlan = %u\n" */
/* 	       "stat->total_tx_bytes_nb = %llu\n" */
/* 	       "stat->total_tx_packet_nb, = %u\n" */
/* 	       "stat->tx_broadcast, = %u\n" */
/* 	       "stat->tx_multicast, = %u\n" */
/* 	       "stat->tx_packet_64_bytes, = %u\n" */
/* 	       "stat->tx_packet_65_127_bytes, = %u\n" */
/* 	       "stat->tx_packet_128_255_bytes, = %u\n" */
/* 	       "stat->tx_packet_256_511_bytes, = %u\n" */
/* 	       "stat->tx_packet_512_1023_bytes, = %u\n" */
/* 	       "stat->tx_packet_1024_1522_bytes, = %u\n" */
/* 	       "stat->tx_packet_1523_9215_bytes = %u\n" */
/* 	       "stat->tx_vlan = %u\n", */
/* 	       stat->total_rx_bytes_nb, */
/* 	       stat->good_rx_bytes_nb, */
/* 	       stat->total_rx_packet_nb, */
/* 	       stat->good_rx_packet_nb, */
/* 	       stat->rx_broadcast, */
/* 	       stat->rx_multicast, */
/* 	       stat->rx_packet_64_bytes, */
/* 	       stat->rx_packet_65_127_bytes, */
/* 	       stat->rx_packet_128_255_bytes, */
/* 	       stat->rx_packet_256_511_bytes, */
/* 	       stat->rx_packet_512_1023_bytes, */
/* 	       stat->rx_packet_1024_1522_bytes, */
/* 	       stat->rx_packet_1523_9215_bytes, */
/* 	       stat->rx_vlan, */
/* 	       stat->total_tx_bytes_nb, */
/* 	       stat->total_tx_packet_nb, */
/* 	       stat->tx_broadcast, */
/* 	       stat->tx_multicast, */
/* 	       stat->tx_packet_64_bytes, */
/* 	       stat->tx_packet_65_127_bytes, */
/* 	       stat->tx_packet_128_255_bytes, */
/* 	       stat->tx_packet_256_511_bytes, */
/* 	       stat->tx_packet_512_1023_bytes, */
/* 	       stat->tx_packet_1024_1522_bytes, */
/* 	       stat->tx_packet_1523_9215_bytes, stat->tx_vlan); */
/* } */

void copy_stat(unsigned int lane_id, mppa_ethernet_lane_stat_t * out)
{
	out->total_rx_bytes_nb.reg =
	    mppabeth_mac_get_total_rx_bytes_nb((void *)
					       &(mppa_ethernet[0]->mac),
					       lane_id);
	out->total_rx_packet_nb.reg =
	    mppabeth_mac_get_total_rx_packet_nb((void *)
						&(mppa_ethernet[0]->mac),
						lane_id);
	out->good_rx_bytes_nb.reg =
	    mppabeth_mac_get_good_rx_bytes_nb((void *)
					      &(mppa_ethernet[0]->mac),
					      lane_id);
	out->good_rx_packet_nb.reg =
	    mppabeth_mac_get_good_rx_packet_nb((void *)
					       &(mppa_ethernet[0]->mac),
					       lane_id);
	out->rx_broadcast.reg =
	    mppabeth_mac_get_rx_broadcast((void *)
					  &(mppa_ethernet[0]->mac),
					  lane_id);
	out->rx_multicast.reg =
	    mppabeth_mac_get_rx_multicast((void *)
					  &(mppa_ethernet[0]->mac),
					  lane_id);
	out->rx_packet_64_bytes.reg =
	    mppabeth_mac_get_rx_packet_64_bytes((void *)
						&(mppa_ethernet[0]->mac),
						lane_id);
	out->rx_packet_65_127_bytes.reg =
	    mppabeth_mac_get_rx_packet_65_127_bytes((void *)
						    &(mppa_ethernet[0]->
						      mac), lane_id);
	out->rx_packet_128_255_bytes.reg =
	    mppabeth_mac_get_rx_packet_128_255_bytes((void *)
						     &(mppa_ethernet[0]->
						       mac), lane_id);
	out->rx_packet_256_511_bytes.reg =
	    mppabeth_mac_get_rx_packet_256_511_bytes((void *)
						     &(mppa_ethernet[0]->
						       mac), lane_id);
	out->rx_packet_512_1023_bytes.reg =
	    mppabeth_mac_get_rx_packet_512_1023_bytes((void *)
						      &(mppa_ethernet[0]->
							mac), lane_id);
	out->rx_packet_1024_1522_bytes.reg =
	    mppabeth_mac_get_rx_packet_1024_1522_bytes((void *)
						       &(mppa_ethernet[0]->
							 mac), lane_id);
	out->rx_packet_1523_9215_bytes.reg =
	    mppabeth_mac_get_rx_packet_1523_9215_bytes((void *)
						       &(mppa_ethernet[0]->
							 mac), lane_id);
	out->rx_vlan.reg =
	    mppabeth_mac_get_rx_vlan((void *) &(mppa_ethernet[0]->mac),
				     lane_id);
	out->total_tx_bytes_nb.reg =
	    mppabeth_mac_get_total_tx_bytes_nb((void *)
					       &(mppa_ethernet[0]->mac),
					       lane_id);
	out->total_tx_packet_nb.reg =
	    mppabeth_mac_get_total_tx_packet_nb((void *)
						&(mppa_ethernet[0]->mac),
						lane_id);
	out->tx_broadcast.reg =
	    mppabeth_mac_get_tx_broadcast((void *)
					  &(mppa_ethernet[0]->mac),
					  lane_id);
	out->tx_multicast.reg =
	    mppabeth_mac_get_tx_multicast((void *)
					  &(mppa_ethernet[0]->mac),
					  lane_id);
	out->tx_packet_64_bytes.reg =
	    mppabeth_mac_get_tx_packet_64_bytes((void *)
						&(mppa_ethernet[0]->mac),
						lane_id);
	out->tx_packet_65_127_bytes.reg =
	    mppabeth_mac_get_tx_packet_65_127_bytes((void *)
						    &(mppa_ethernet[0]->
						      mac), lane_id);
	out->tx_packet_128_255_bytes.reg =
	    mppabeth_mac_get_tx_packet_128_255_bytes((void *)
						     &(mppa_ethernet[0]->
						       mac), lane_id);
	out->tx_packet_256_511_bytes.reg =
	    mppabeth_mac_get_tx_packet_256_511_bytes((void *)
						     &(mppa_ethernet[0]->
						       mac), lane_id);
	out->tx_packet_512_1023_bytes.reg =
	    mppabeth_mac_get_tx_packet_512_1023_bytes((void *)
						      &(mppa_ethernet[0]->
							mac), lane_id);
	out->tx_packet_1024_1522_bytes.reg =
	    mppabeth_mac_get_tx_packet_1024_1522_bytes((void *)
						       &(mppa_ethernet[0]->
							 mac), lane_id);
	out->tx_packet_1523_9215_bytes.reg =
	    mppabeth_mac_get_tx_packet_1523_9215_bytes((void *)
						       &(mppa_ethernet[0]->
							 mac), lane_id);
	out->tx_vlan.reg =
	    mppabeth_mac_get_tx_vlan((void *) &(mppa_ethernet[0]->mac),
				     lane_id);
}

int compare_stat(mppa_ethernet_lane_stat_t * a,
		 mppa_ethernet_lane_stat_t * b)
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
		b->tx_packet_1523_9215_bytes.reg
		&& a->tx_vlan.reg == b->tx_vlan.reg);
}



#endif
#endif				/* IO_UTILS_H */
