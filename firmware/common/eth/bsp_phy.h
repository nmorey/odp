/*
 * Copyright (C) 2008-2014 Kalray SA.
 *
 * All rights reserved.
 */
#ifndef _bsp_phy_h_
#define _bsp_phy_h_

#include <HAL/hal/hal.h>
#include <mppa_bsp.h>

#ifdef DEBUG_ETH_PLL
#  include <stdio.h>
#endif

#define PHY_SERDES_PSTATE_P0  0
#define PHY_SERDES_PSTATE_P0S 1
#define PHY_SERDES_PSTATE_P1  2
#define PHY_SERDES_PSTATE_P2  3

#ifndef clear_ethernet_phy_ctrl_reset
static __inline__ void
clear_ethernet_phy_ctrl_reset(void)
{
	__k1_io_write32((void*)&mppa_ethernet[0]->mac.reset_clear.word, 1);
}
#endif

#ifndef clear_ethernet_phy_serdes_reset
static __inline__ void
clear_ethernet_phy_serdes_reset(void)
{
	__k1_io_write32((void*)&mppa_ethernet[0]->mac.reset_clear.word, 2);
}
#endif

#ifndef set_ethernet_phy_ctrl_reset
static __inline__ void
set_ethernet_phy_ctrl_reset(void)
{
	__k1_io_write32((void*)&mppa_ethernet[0]->mac.reset_set.word, 1);
}
#endif

#ifndef set_ethernet_phy_serdes_reset
static __inline__ void
set_ethernet_phy_serdes_reset(void)
{
	__k1_io_write32((void*)&mppa_ethernet[0]->mac.reset_set.word, 2);
}
#endif

#ifndef get_ethernet_phy_serdes_reset
static __inline__ __k1_int32_t
get_ethernet_phy_serdes_reset(void)
{
	mppa_ethernet_mac_reset_set_t reset_set;
	reset_set.word = __k1_io_read32((void*)&mppa_ethernet[0]->mac.reset_set.word);
	return reset_set._.phy;
}
#endif

#ifndef get_ethernet_phy_ctrl_reset
static __inline__ __k1_int32_t
get_ethernet_phy_ctrl_reset(void)
{
	mppa_ethernet_mac_reset_set_t reset_set;
	reset_set.word = __k1_io_read32((void*)&mppa_ethernet[0]->mac.reset_set.word);
	return reset_set._.phy_ctrl;
}
#endif

#ifndef __k1_phy_lane_reset
static __inline__ void
__k1_phy_lane_reset(__k1_uint8_t lane_id)
{
	__k1_io_write32((void*)&mppa_eth_pcie_phy[0]->lane_pwr_ctrl[lane_id].word, 0x0500);
	__k1_io_write32((void*)&mppa_eth_pcie_phy[0]->rx_cfg_0[lane_id].word, 0x2406);
}
#endif

enum {
	MPPA_PERIPH_CLOCK_100   = 100,
	MPPA_PERIPH_CLOCK_156   = 156,
};

__k1_uint8_t
__k1_phy_configure_ber(__k1_uint8_t phy_mode, __k1_uint32_t lane_valid, __k1_uint32_t ber_rate);
void
__k1_phy_unconfigure_ber(__k1_uint8_t phy_mode, __k1_uint32_t lane_valid);

__k1_uint8_t
__k1_phy_init_full(__k1_uint8_t phy_mode, __k1_uint8_t bw_div, int periph_clock);

__k1_uint32_t
__k1_phy_toggle_loopback(__k1_uint32_t phy_mode, int loopback_mode);

__k1_uint32_t
__k1_phy_polarity_reverse(__k1_uint8_t phy_mode, __k1_uint8_t rx_lane_valid, __k1_uint8_t tx_lane_valid);

__k1_uint32_t
__k1_phy_monitor_core_clk_on_dmon(__k1_uint8_t phy_mode);

/**
 * \fn static __inline__ __k1_uint32_t __k1_phy_enable_loopback( __k1_uint32_t phy_mode )
 * \brief Enable PHY loopback for Ethernet, PCIe or Interlaken
 * \param[in] phy_mode PHY mode to configure
 * \return PHY loopback enabling status
 */
static __inline__ __k1_uint32_t
__k1_phy_enable_loopback(__k1_uint32_t phy_mode)
{
	return __k1_phy_toggle_loopback(phy_mode, 1);
}

static __inline__ __k1_uint32_t
__k1_phy_pcs_enable_loopback(__k1_uint32_t phy_mode)
{
	return __k1_phy_toggle_loopback(phy_mode, 3);
}

/**
 * \fn static __inline__ __k1_uint32_t __k1_phy_disable_loopback( __k1_uint32_t phy_mode )
 * \brief Disable PHY loopback for Ethernet, PCIe or Interlaken
 * \param[in] phy_mode PHY mode to configure
 * \return PHY loopback disabling status
 */
static __inline__ __k1_uint32_t
__k1_phy_disable_loopback(__k1_uint32_t phy_mode)
{
	return __k1_phy_toggle_loopback(phy_mode, 0);
}

static __inline__ __k1_uint32_t
__k1_phy_pcs_disable_loopback(__k1_uint32_t phy_mode)
{
	return __k1_phy_toggle_loopback(phy_mode, 2);
}

#endif /* _bsp_phy_h_ */
