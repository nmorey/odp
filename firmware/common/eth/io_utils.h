#ifndef IO_UTILS_H
#define IO_UTILS_H
#ifdef __k1io__
#include "utils.h"
#include "bsp_i2c.h"
#include "bsp_phy.h"
#include <HAL/hal/hal.h>
#include "mdio_88E1111.h"
#include "top_88E1111.h"
#include "mppa_eth_mac.h"
#include "mppa_eth_phy_core.h"
#include "mppa_eth_loadbalancer.h"


/** Set the duplexwhen using the 1G link MPPABETHMAC_MODE_1G */ 
#define MPPABETHMAC_FULLDUPLEX 	1
#define MPPABETHMAC_HALFDUPLEX 	0

enum mppa_eth_mac_duplex_e {
	MPPA_ETH_MAC_FULLDUPLEX  = 	MPPABETHMAC_FULLDUPLEX,
	MPPA_ETH_MAC_HALFDUPLEX  =	MPPABETHMAC_HALFDUPLEX,
};


#define MAX_TABLE_IDX		 0	/*ONly true on fpga */
#define DEFAULT_TABLE_IDX 	 8
enum loopback_level { MAC_LOCAL_LOOPBACK = 0, MAC_BYPASS_LOOPBACK, LAST_LOOPBACK_LEVEL
};

/**
 * @brief Conversion from macmode value to phy compatible mode value
 * @param mac_mode the mac_mode to convert
 * @return the equivalent phy_mode
 */
static inline
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

static
volatile mppa_i2c_master_t *ethernet_i2c_master = NULL;

static inline
int mppa_eth_i2c_read(uint8_t chip_id, void *uarg, unsigned reg, uint16_t * pval)
{
	UNUSED(uarg);
	return mppa_i2c_register_read((void *) ethernet_i2c_master, chip_id, reg, pval);
}

static inline
int mppa_eth_i2c_write(uint8_t chip_id, void *uarg, unsigned reg, uint16_t val)
{
	UNUSED(uarg);
	return mppa_i2c_register_write((void *) ethernet_i2c_master, chip_id, reg, val);
}

static inline
void set_no_fcs()
{
	mppabeth_mac_disable_rx_fcs_deletion((void *)
					     &(mppa_ethernet[0]->mac));
	mppabeth_mac_disable_tx_fcs_insertion((void *)
					      &(mppa_ethernet[0]->mac));
}

static inline
void set_fcs()
{
	mppabeth_mac_enable_rx_fcs_deletion((void *)
					    &(mppa_ethernet[0]->mac));
	mppabeth_mac_enable_tx_fcs_insertion((void *)
					     &(mppa_ethernet[0]->mac));
}

/**
 * @brief Setup the specified lane (phy must be corectly initialised before)
 * @param lane_id The lane to start
 */
void start_lane(unsigned int lane_id, enum mppa_eth_mac_ethernet_mode_e mode);

/**
 * @brief Check if a MAC is up
 * @return 0 on success
 * @retval -EINVAL Invalidate lane
 * @retval -ENETDOWN Lane down
 * @retval -EIO Lane not initialized
 */
int mac_poll_state(unsigned int lane_id, enum mppa_eth_mac_ethernet_mode_e mode);

/**
 * @brief Complete mac initialization =>
 * - synchro with the PHY
 * - startup the lane
 * - Check the lane status
 * If the provided mode is -1, mode is set to the defautl value expected
 *  on the selected lane
 * @return Status
 * @retval -EINVAL Invalid parameters or mode not compatible with previous init
 * @retval -EBUSY Interface is already open and up
 * @retval -ENOSYS Mode not supported yet
 * @retval -EIO Issue with Phy or I2C initialization
 * @retval -ENETDOWN Init successful but link is still down
 * @retval 0 Success
 */
uint32_t init_mac(int lane_id, enum mppa_eth_mac_ethernet_mode_e mode);

/**
 * @brief Complete mac initialization with speficied rate and duplex=>
 * - synchro with the PHY
 * - startup the lane
 * - Disable autoneg
 * - Set rate and duplex
 * - Check the lane status
 * @return Status
 * @retval -EINVAL Invalid parameters or mode not compatible with previous init
 * @retval -EBUSY Interface is already open and up
 * @retval -ENOSYS Mode not supported yet
 * @retval -EIO Issue with Phy or I2C initialization
 * @retval -ENETDOWN Init successful but link is still down
 * @retval 0 Success
 */
uint32_t init_mac_1G_without_autoneg(int lane_id, enum mppa_eth_mac_1G_mode_e rate_1G, enum mppa_eth_mac_duplex_e duplex_mode);

void dump_reg_dev(uint8_t dev);

void configure_dnoc_ethernet_lb(uint32_t ethernet_dnoc_interface,
								uint32_t dnoc_interface,
								uint32_t tx_id_for_mac,
								uint64_t first_dir_for_rx_packets,
								uint64_t route_for_rx_packets,
								uint16_t context,
								uint64_t min_tag_for_rx_packets, uint64_t max_tag_for_rx_packets);
void configure_dnoc_ethernet_tx(uint32_t dnoc_interface,
								uint32_t rx_tag_to_mac, uint32_t lane_id, uint32_t fifo_id);
void dump_stat(mppa_ethernet_lane_stat_t * stat);
void copy_stat(unsigned int lane_id, mppa_ethernet_lane_stat_t * out);
int compare_stat(mppa_ethernet_lane_stat_t * a, mppa_ethernet_lane_stat_t * b);
int dump_registers();


#endif
#endif				/* IO_UTILS_H */
