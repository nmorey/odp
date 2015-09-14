/*
 * Copyright (C) 2008-2014 Kalray SA.
 *
 * All rights reserved.
 */
#include "bsp_phy.h"

unsigned int __k1_phy_bit_swap(unsigned int reg_value, unsigned int reg_length)
{
  unsigned int res = 0;
  unsigned int i;

  for(i=0;i<reg_length;i++) {
    if(((reg_value >> i) & 1) == 1) res |= (1 << (reg_length - 1 - i));
  }
  return res;
}

unsigned int __k1_phy_get_mac_sds_core_div(unsigned int phy_id) {
  return __k1_phy_bit_swap(mppa_eth_pcie_csr[phy_id]->mac_sds_core_div.word, 9);
}

void __k1_phy_set_mac_sds_core_div(unsigned int phy_id, unsigned int mac_sds_core_div) {
  mppa_eth_pcie_csr[phy_id]->mac_sds_core_div.word = __k1_phy_bit_swap(mac_sds_core_div, 9);
}

/**
 * \fn static __inline__ void __k1_phy_configure_ber( __k1_uint32_t lane_valid, __k1_uint32_t ber_rate )
 * \brief Configuration of bit error injection
 * \param[in] lane_valid Lane to be enabled with BER
 * \param[in] ber_rate BER rate between 0 and 255 (0: very low BER; 255: very high BER)
 */
__k1_uint8_t
__k1_phy_configure_ber(__k1_uint8_t phy_mode, __k1_uint32_t lane_valid, __k1_uint32_t ber_rate)
{
  __k1_int32_t i;
  mppa_eth_pcie_phy_pcs_ber_cfg_1_t local_pcs_ber_cfg_1;
  mppa_eth_pcie_phy_pcs_ber_cfg_0_t local_pcs_ber_cfg_0;
  mppa_eth_pcie_phy_pcs_ber_const_pat_0_t local_pcs_ber_const_pat_0;
  mppa_eth_pcie_phy_pcs_ber_const_pat_1_t local_pcs_ber_const_pat_1;

  local_pcs_ber_cfg_1.word = 0;
  // Activate random error mode
  local_pcs_ber_cfg_1._.ber_pat_sel = 0;
  // SImple static error position: will be ignored in random error mode
  local_pcs_ber_cfg_1._.ber_bit_sel = 0;
  // Sub-timer divider
  local_pcs_ber_cfg_0.word = 0;
  // Set bit sel: must be inversely proportional to BER rate
  local_pcs_ber_cfg_0._.ber_rate_exp_sel = ((0xFF - ber_rate) & 0xFF) >> 3; // 5 bits
  // Instant static insertion OFF: will be ignored in random error mode
  local_pcs_ber_cfg_0._.ber_const_rate = 0;
  // LFSR ON
  local_pcs_ber_cfg_0._.ber_rate_mode = 0;
  // Do not perform immediate error insertion
  // FIXME: This bit is not available
  // local_pcs_ber_cfg_0._.force_ber = 0;
  // Enable BER mode
  local_pcs_ber_cfg_0._.ber_enable = 1;
  //
  local_pcs_ber_const_pat_0.word                   = 0;
  local_pcs_ber_const_pat_0._.ber_const_pat_19_16_ = 0x8;

  switch (phy_mode) {
  case MPPA_ETH_PCIE_PHY_MODE_PCIE_ILK:
  case MPPA_ETH_PCIE_PHY_MODE_PCIE:
    for (i = 1; i < 3; i++) {
      // LFSR vector parameters
      local_pcs_ber_const_pat_1.word = __k1_io_read32((void*)&mppa_eth_pcie_phy[i]->pcs_ber_const_pat_1.word);
      local_pcs_ber_const_pat_1._.ber_const_pat_15_0_ = 0x8001;
      __k1_io_write32((void*)&mppa_eth_pcie_phy[i]->pcs_ber_const_pat_1.word, local_pcs_ber_const_pat_1.word);
      __k1_io_write32((void*)&mppa_eth_pcie_phy[i]->pcs_ber_const_pat_0.word, local_pcs_ber_const_pat_0.word);
    }
    for (i = 0; i < 4; i++) {
      if (((lane_valid >> i) & 0x1) != 0) {
        __k1_io_write32((void*)&mppa_eth_pcie_phy[2]->pcs_ber_cfg_1[i].word, local_pcs_ber_cfg_1.word);
        __k1_io_write32((void*)&mppa_eth_pcie_phy[2]->pcs_ber_cfg_0[i].word, local_pcs_ber_cfg_0.word);
      }
      if (((lane_valid >> (i + 4)) & 0x1) != 0) {
        __k1_io_write32((void*)&mppa_eth_pcie_phy[1]->pcs_ber_cfg_1[i].word, local_pcs_ber_cfg_1.word);
        __k1_io_write32((void*)&mppa_eth_pcie_phy[1]->pcs_ber_cfg_0[i].word, local_pcs_ber_cfg_0.word);
      }
    }
    break;
  case MPPA_ETH_PCIE_PHY_MODE_ETH_ILK:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_40G:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_BASE_R:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_XAUI:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_RXAUI:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_1G:
    local_pcs_ber_const_pat_1.word = __k1_io_read32((void*)&mppa_eth_pcie_phy[0]->pcs_ber_const_pat_1.word);
    local_pcs_ber_const_pat_1._.ber_const_pat_15_0_ = 0x8001;
    __k1_io_write32((void*)&mppa_eth_pcie_phy[0]->pcs_ber_const_pat_1.word, local_pcs_ber_const_pat_1.word);
    __k1_io_write32((void*)&mppa_eth_pcie_phy[0]->pcs_ber_const_pat_0.word, local_pcs_ber_const_pat_0.word);
    for (i = 0; i < 4; i++) {
      if (((lane_valid >> i) & 0x1) != 0) {
        __k1_io_write32((void*)&mppa_eth_pcie_phy[0]->pcs_ber_cfg_1[i].word, local_pcs_ber_cfg_1.word);
        __k1_io_write32((void*)&mppa_eth_pcie_phy[0]->pcs_ber_cfg_0[i].word, local_pcs_ber_cfg_0.word);
      }
    }
    break;
  default:
    return 0x00;
    break;
  }
  return 0x01;
}

/**
 * \fn static __inline__ void __k1_phy_unconfigure_ber( __k1_uint32_t lane_valid )
 * \brief Unconfiguration of some lanes of bit error injection
 * \param[in] lane_valid Lane to be disabled with BER
 */
void
__k1_phy_unconfigure_ber(__k1_uint8_t phy_mode, __k1_uint32_t lane_valid)
{
  __k1_int32_t i;

  switch (phy_mode) {
  case MPPA_ETH_PCIE_PHY_MODE_PCIE_ILK:
  case MPPA_ETH_PCIE_PHY_MODE_PCIE:
    for (i = 0; i < 4; i++) {
      if (((lane_valid >> (i + 4)) & 0x1) != 0) {
        mppa_eth_pcie_phy_pcs_ber_cfg_0_t pcs_ber_cfg_0;
        pcs_ber_cfg_0.word          = __k1_io_read32((void*)&mppa_eth_pcie_phy[1]->pcs_ber_cfg_0[i].word);
        pcs_ber_cfg_0._.ber_enable  = 0;
        __k1_io_write32((void*)&mppa_eth_pcie_phy[1]->pcs_ber_cfg_0[i].word, pcs_ber_cfg_0.word);
      }
      if (((lane_valid >> i) & 0x1) != 0) {
        mppa_eth_pcie_phy_pcs_ber_cfg_0_t pcs_ber_cfg_0;
        pcs_ber_cfg_0.word          = __k1_io_read32((void*)&mppa_eth_pcie_phy[2]->pcs_ber_cfg_0[i].word);
        pcs_ber_cfg_0._.ber_enable  = 0;
        __k1_io_write32((void*)&mppa_eth_pcie_phy[2]->pcs_ber_cfg_0[i].word, pcs_ber_cfg_0.word);
      }
    }
    break;
  case MPPA_ETH_PCIE_PHY_MODE_ETH_ILK:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_40G:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_BASE_R:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_XAUI:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_RXAUI:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_1G:
    for (i = 0; i < 4; i++) {
      if (((lane_valid >> i) & 0x1) != 0) {
        mppa_eth_pcie_phy_pcs_ber_cfg_0_t pcs_ber_cfg_0;
        pcs_ber_cfg_0.word          = __k1_io_read32((void*)&mppa_eth_pcie_phy[0]->pcs_ber_cfg_0[i].word);
        pcs_ber_cfg_0._.ber_enable  = 0;
        __k1_io_write32((void*)&mppa_eth_pcie_phy[0]->pcs_ber_cfg_0[i].word, pcs_ber_cfg_0.word);
      }
    }
    break;
  default:
    break;
  }
}

/**
 * \fn static __inline__ __k1_uint8_t __k1_phy_init_full( __k1_uint8_t phy_mode __k1_uint8_t bw_div, int periph_clock)
 * \brief Initialization of PHY for Ethernet, PCIe or Interlaken
 * \param[in] phy_mode PHY mode to initialize
 * \param[in] bw_div Bandwidth division
 * \param[in] periph_clock Peripheral clock, supports 100 and 156 Mhz (MPPA_PERIPH_CLOCK_156 or MPPA_PERIPH_CLOCK_100)
 * \return PHY initialization status
 */
__k1_uint8_t 
__k1_phy_init_full(__k1_uint8_t phy_mode, __k1_uint8_t bw_div, int periph_clock)
{
  unsigned int i;
  unsigned int phy_id;
  mppa_ftu_pcie_reset_t pcie_reset;
  mppa_eth_pcie_csr_pll0_ctrl_t pll0_ctrl;
  mppa_eth_pcie_csr_pll1_ctrl_t pll1_ctrl;
  mppa_eth_pcie_csr_smpl_rate_ctrl_t smpl_rate_ctrl;
  mppa_eth_pcie_csr_lane_param_t lane_param;
  mppa_eth_pcie_csr_serdes_ctrl_t serdes_ctrl;
  mppa_eth_pcie_csr_lane_ctrl_t lane_ctrl;
  mppa_eth_pcie_phy_pcs_misc_cfg_0_t pcs_misc_cfg_0;
  mppa_eth_pcie_csr_lane_mode_t lane_mode;

  /**********************************************************************************/
  // Reset PHY serdes and clear PHY ctrl
  /**********************************************************************************/
 #ifdef DEBUG_ETH_PLL
  printf("Starting PLL configuration.\n");
 #endif
  switch (phy_mode) {
  case MPPA_ETH_PCIE_PHY_MODE_ETH_ILK:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_40G:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_BASE_R:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_XAUI:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_RXAUI:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_1G:
    phy_id = 0;
    mppa_ethernet[0]->mac.reset_set.word = 0x7;
    clear_ethernet_phy_ctrl_reset();
    break;
  case MPPA_ETH_PCIE_PHY_MODE_PCIE_ILK:
    phy_id = 2;
    if (__bsp_flavour != BSP_EXPLORER && __bsp_flavour != BSP_ETH_530) {
      pcie_reset.word = __k1_io_read32((void*)&mppa_ftu[0]->pcie_reset.word);
      pcie_reset._.pcie_csr_sw    = 0;
      pcie_reset._.pcie_phy_sw    = 0;
      pcie_reset._.pcie_csr_sw    = 1;
      __k1_io_write32((void*)&mppa_ftu[0]->pcie_reset.word, pcie_reset.word);
    }
    break;
  case MPPA_ETH_PCIE_PHY_MODE_PCIE:
    phy_id          = 2;
    pcie_reset.word = __k1_io_read32((void*)&mppa_ftu[0]->pcie_reset.word);
    // Mode par defaut en sortie de reset : il suffit donc de refaire un reset du phy !!
    pcie_reset._.pcie_phy_sw    = 0;
    pcie_reset._.pcie_csr_sw    = 0;
    // On attend quelques cycle que le reset se propage puis on le relache.
    pcie_reset._.pcie_csr_sw    = 1;
    pcie_reset._.pcie_phy_sw    = 1;
    __k1_io_write32((void*)&mppa_ftu[0]->pcie_reset.word, pcie_reset.word);
    break;
  default:        // Unknow mode.
    return 0x00;
    break;
  }
  // On doit attendre la propagation du reset avant d'aller plus loin.
  __builtin_k1_fence();
  if (phy_mode == MPPA_ETH_PCIE_PHY_MODE_PCIE) return 0x01;

  /**********************************************************************************/
  // Configuration de la PLL.
  /**********************************************************************************/
  // On peut commencer la programmation du phy
 #ifdef DEBUG_ETH_PLL
  printf("Start PHY PLL configuration\n");
 #endif

  if (__bsp_flavour != BSP_EXPLORER && __bsp_flavour != BSP_ETH_530) {
    switch (phy_mode) {
    // En mode interlaken, la bande passante cumulee de l'ensemble des lanes serdes doit etre inferieure a la bande passante du l'interface lbus du controleur.
    // Sinon, le controleur interlaken passe en overflow/underflow.
    // Interface du controleur : 128 bits @ 400 MHz => 6.25 GB/s
    // Bande passante max d'une lane serdes : 20 bits @ 516 MHz => 1.26 GB/s
    // Si 4 lanes actives (Ethernet) => bande passante 4 x lanes = 5.04 GB/s  < 6.25 GB/s : tout va bien
    // Si 8 lanes actives (PCIe)     => bande passante 8 x lanes = 10.08 GB/s > 6.25 GB/s : il faut ralentir les lanes
    // => 6.25 GB/s / 8 = 800 MB/s => 20 bits @ 320MHz
    case MPPA_ETH_PCIE_PHY_MODE_PCIE_ILK:
      for(i=1;i<3;i++) {
        pll0_ctrl.word = __k1_io_read32((void*)&mppa_eth_pcie_csr[i]->pll0_ctrl.word);
        // padref clk freq : 100,00 MHz, serdes clk freq : 20 x 320,000 MHz - mode double front - 10 x 320,000 MHz requis
        pll0_ctrl._.pll0_div        = 31;                                           // pll0_div    => (31 + 1)           * 100 MHz = 10 x 320,000 MHz
        pll0_ctrl._.pll0_pcs_div    = 9;                                            // serdes freq => (31 + 1) / (9 + 1) * 100 MHz =      320,000 MHz
        __k1_io_write32((void*)&mppa_eth_pcie_csr[i]->pll0_ctrl.word, pll0_ctrl.word);
        __k1_phy_set_mac_sds_core_div(i, 1);   // Cette clock ne sert pas : on met la frequence la plus basse possible afin de minimiser la conso
      }
      break;
    case MPPA_ETH_PCIE_PHY_MODE_ETH_ILK:
      pll0_ctrl.word = __k1_io_read32((void*)&mppa_eth_pcie_csr[phy_id]->pll0_ctrl.word);
      // padref clk freq : 156,25 MHz, serdes clk freq : 20 x 515,625 MHz - mode double front - 10 x 515,625 MHz requis
      if (periph_clock == MPPA_PERIPH_CLOCK_156) {
        pll0_ctrl._.pll0_div = 32;  // pll0_div    => (32 + 1)           * 156,25 MHz = 10 x 515,625 MHz
      } else if (periph_clock == MPPA_PERIPH_CLOCK_100) {
        // clock 100MHz, to get to 5GHz need x50
        pll0_ctrl._.pll0_div = 49;  // pll0_div    => (49 + 1)           * 100,00 MHz = 50 x 100,00 MHz
      }
      switch (bw_div) {
      case 1:
        pll0_ctrl._.pll0_pcs_div = 9;   // serdes freq => (31 + 1) / (9 + 1) * freq =      515,625 MHz (@156,25 MHz)
        break;
      case 2:
        pll0_ctrl._.pll0_pcs_div = 19;  // serdes freq => (31 + 1) / (19 + 1) * freq =      257.8125 MHz (@156,25 MHz)
        break;
      case 4:
        pll0_ctrl._.pll0_pcs_div = 39;  // serdes freq => (31 + 1) / (39 + 1) * freq =      125 MHz (@156,25 MHz)
        break;
      case 8:
        pll0_ctrl._.pll0_pcs_div = 79;  // serdes freq => (31 + 1) / (79 + 1) * freq =     62.5 MHz (@156,25 MHz)
        break;
      default:
        return 0x00;
        break;
      }
      __k1_io_write32((void*)&mppa_eth_pcie_csr[phy_id]->pll0_ctrl.word, pll0_ctrl.word);
      __k1_phy_set_mac_sds_core_div(phy_id, 1); // cette clock ne sert pas : on met la frequence la plus basse possible afin de minimiser la conso
      break;
    case MPPA_ETH_PCIE_PHY_MODE_ETH_40G:
      // padref clk freq : 156,25 MHz, core clk freq : 644,531 MHz, serdes clk freq : 20 x 515,625 MHz - mode double front - 10 x 515,625 MHz requis
      pll0_ctrl.word              = __k1_io_read32((void*)&mppa_eth_pcie_csr[phy_id]->pll0_ctrl.word);
                        pll0_ctrl._.pll0_div        = 32; // VCO freq    = (32 + 1)           * 156,25 MHz = 10 x 515,625 MHz
                        pll0_ctrl._.pll0_pcs_div    = 9;  // serdes freq = (32 + 1) / (9 + 1) * 156,25 MHz = 515,625 MHz
                        if(bw_div == 2) pll0_ctrl._.pll0_pcs_div = 19;
                        if(bw_div == 4) pll0_ctrl._.pll0_pcs_div = 39;
                        if(bw_div == 8) pll0_ctrl._.pll0_pcs_div = 79;
      __k1_io_write32((void*)&mppa_eth_pcie_csr[phy_id]->pll0_ctrl.word, pll0_ctrl.word);
      __k1_phy_set_mac_sds_core_div(phy_id, 8);  // mac freq => (32 + 1) / 8 * 156,25 MHz = 644,531 MHz
      break;
    case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_BASE_R:
      pll0_ctrl.word = __k1_io_read32((void*)&mppa_eth_pcie_csr[phy_id]->pll0_ctrl.word);
      // padref clk freq : 156,25 MHz, core clk freq : 161,133 MHz, serdes clk freq : 20 x 515,625 MHz - mode double front - 10 x 515,625 MHz requis
      pll0_ctrl._.pll0_div        = 32;                                               // pll0_div    => (32 + 1)           * 156,25 MHz = 10 x 515,625 MHz
      pll0_ctrl._.pll0_pcs_div    = 9;                                                // serdes freq => (32 + 1) / (9 + 1) * 156,25 MHz =      515,625 MHz
                        if(bw_div == 2) pll0_ctrl._.pll0_pcs_div = 19;
                        if(bw_div == 4) pll0_ctrl._.pll0_pcs_div = 39;
                        if(bw_div == 8) pll0_ctrl._.pll0_pcs_div = 79;
      __k1_io_write32((void*)&mppa_eth_pcie_csr[phy_id]->pll0_ctrl.word, pll0_ctrl.word);
      __k1_phy_set_mac_sds_core_div(phy_id, 32); // mac freq => (32 + 1) / (32) * 156,25 MHz = 161,133 MHz
      break;
    case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_XAUI:
      // padref clk freq : 156,25 MHz, core clk freq : 164,474 MHz, serdes clk freq : 20 x 156,25 MHz - mode double front - 10 x 156,25 MHz requis
      pll0_ctrl.word              = __k1_io_read32((void*)&mppa_eth_pcie_csr[phy_id]->pll0_ctrl.word);
      pll0_ctrl._.pll0_div        = 19;                                               // pll0_div    => (19 + 1)            * 156,25 MHz = 10 x 312,5 MHz
      pll0_ctrl._.pll0_pcs_div    = 19;                                               // serdes freq => (19 + 1) / (19 + 1) * 156,25 MHz = 156,25     MHz
      __k1_io_write32((void*)&mppa_eth_pcie_csr[phy_id]->pll0_ctrl.word, pll0_ctrl.word);
      __k1_phy_set_mac_sds_core_div(phy_id, 19);  // mac freq => (19 + 1) / (19) * 156,25 MHz = 164,47 MHz
      break;
    case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_RXAUI:
      // padref clk freq : 156,25 MHz, core clk freq : 160,256 MHz, serdes clk freq : 20 x 312,5 MHz - mode double front - 10 x 312,5 MHz requis
      pll0_ctrl.word              = __k1_io_read32((void*)&mppa_eth_pcie_csr[phy_id]->pll0_ctrl.word);
      pll0_ctrl._.pll0_div        = 39;                                               // pll0_div    => (39 + 1)            * 156,25 MHz = 10 x 625 MHz
      pll0_ctrl._.pll0_pcs_div    = 19;                                               // serdes freq => (39 + 1) / (19 + 1) * 156,25 MHz = 312,5    MHz
      __k1_io_write32((void*)&mppa_eth_pcie_csr[phy_id]->pll0_ctrl.word, pll0_ctrl.word);
      __k1_phy_set_mac_sds_core_div(phy_id, 39);   // mac freq => (39 + 1) / (39) * 156,25 MHz = 160,256 MHz
      break;
    case MPPA_ETH_PCIE_PHY_MODE_ETH_1G:
      pll0_ctrl.word = __k1_io_read32((void*)&mppa_eth_pcie_csr[phy_id]->pll0_ctrl.word);
      // padref clk freq : 156,25 MHz, core clk freq : 125 MHz, serdes clk freq : 10 x 125 MHz = 1,250 GHz
      pll0_ctrl._.pll0_div        = 31;                                               // pll0_div    => 4 x 10 x 125 MHz = (31 + 1)            * 156,25 MHz
      pll0_ctrl._.pll0_pcs_div    = 39;                                               // serdes freq =>          125 MHz = (31 + 1) / (39 + 1) * 156,25 MHz
      __k1_io_write32((void*)&mppa_eth_pcie_csr[phy_id]->pll0_ctrl.word, pll0_ctrl.word);
      __k1_phy_set_mac_sds_core_div(phy_id, 40);  // mac freq => 125 MHz = (31 + 1) / 40 * 156,25 MHz
      break;
    default:
      /* Re-enable interrupt */
      return 0x00;
    }
  }

  // PLL operating range : pll_op_range : 0x00 => pll_freq <  5GHz
  //                                      0x01 => pll_freq >= 5GHz
  // Refclk_select : pad => pll_refclk_freq_sel = 0x00
  if (phy_mode == MPPA_ETH_PCIE_PHY_MODE_PCIE_ILK) {
 #ifdef DEBUG_ETH_PLL
    printf("Switching PLL clock tree to ILK configuration\n");
 #endif
    pll1_ctrl.word  = __k1_io_read32((void*)&mppa_eth_pcie_csr[1]->pll1_ctrl.word);
    pll1_ctrl.word  |= 0x00000100;  // pll1_ctrl._.pll_op_range        = 0x01; // pll_op_range
    pll1_ctrl.word  &= 0xfeffffff;  // pll1_ctrl._.pll_refclk_freq_sel = 0x00; // pll_refclk_freq_sel
    __k1_io_write32((void*)&mppa_eth_pcie_csr[1]->pll1_ctrl.word, pll1_ctrl.word);
  }
  pll1_ctrl.word = __k1_io_read32((void*)&mppa_eth_pcie_csr[phy_id]->pll1_ctrl.word);
  if (phy_mode == MPPA_ETH_PCIE_PHY_MODE_ETH_10G_XAUI) {
    pll1_ctrl.word &= 0xfffffeff;   // pll1_ctrl._.pll_op_range = 0x00; // pll_op_range
  } else {
    pll1_ctrl.word |= 0x00000100;   // pll1_ctrl._.pll_op_range = 0x01; // pll_op_range
  }
  pll1_ctrl.word &= 0xfeffffff;       // pll1_ctrl._.pll_refclk_freq_sel = 0x00; // pll_refclk_freq_sel
  if (phy_mode == MPPA_ETH_PCIE_PHY_MODE_ETH_ILK && bw_div >= 10) {
    pll1_ctrl.word &= 0xfffffeff;   // pll1_ctrl._.pll_op_range = 0x00; // pll_op_range
  }
  if (phy_mode == MPPA_ETH_PCIE_PHY_MODE_ETH_ILK && bw_div == 16) {
    pll1_ctrl.word |= 0x00010000;   // pll1_ctrl._.pll_hdr_sel = 1;
  }
  // pll1_ctrl.word |= 0x00010000; //pll1_ctrl._.pll_hdr_sel = 1;
  // pll1_ctrl.word |= 0x01000000;//pll1_ctrl._.pll_refclk_freq_sel = 0x01; // pll_refclk_freq_sel

  __k1_io_write32((void*)&mppa_eth_pcie_csr[phy_id]->pll1_ctrl.word, pll1_ctrl.word);

  // smpl_rate = 0x00
  // op_range = 0x00 si non DFE, 0x01 si DFE (adaptive decision feedback equalizer)
  // DFE a desactiver pour des economies d'energie..mais perte de fiabilite...
  // dfe_train_ok = 0x00
  // rx_polarity  = 0x00
  if (__bsp_flavour != BSP_EXPLORER && __bsp_flavour != BSP_ETH_530) {
    if (phy_mode == MPPA_ETH_PCIE_PHY_MODE_PCIE_ILK) {
      smpl_rate_ctrl.word         = __k1_io_read32((void*)&mppa_eth_pcie_csr[1]->smpl_rate_ctrl.word);
      smpl_rate_ctrl._.smpl_rate  = 0x00; // smpl_rate
      /* if(phy_mode == MPPA_ETH_PCIE_PHY_MODE_ETH_ILK && bw_div == 16) { */
      /*   smpl_rate_ctrl._.smpl_rate    = 0x01; // smpl_rate */
      /* } */
      smpl_rate_ctrl._.op_range       = 0x00; // op_range
      smpl_rate_ctrl._.dfe_train_ok   = 0x00; // dfe_train_ok
      smpl_rate_ctrl._.rx_polarity    = 0x00; // rx_polarity
      __k1_io_write32((void*)&mppa_eth_pcie_csr[1]->smpl_rate_ctrl.word, smpl_rate_ctrl.word);
    }
    smpl_rate_ctrl.word             = __k1_io_read32((void*)&mppa_eth_pcie_csr[phy_id]->smpl_rate_ctrl.word);
    smpl_rate_ctrl._.smpl_rate      = 0x00; // smpl_rate
    smpl_rate_ctrl._.op_range       = 0x00; // op_range
    smpl_rate_ctrl._.dfe_train_ok   = 0x00; // dfe_train_ok
    smpl_rate_ctrl._.rx_polarity    = 0x00; // rx_polarity
    __k1_io_write32((void*)&mppa_eth_pcie_csr[phy_id]->smpl_rate_ctrl.word, smpl_rate_ctrl.word);

    // pcie/pcie_width = 0x00
    // Comma detect  : rx_encdet = 0x00
    // Signal detect : rx_sigdet = 0x01
    // lane_pwr_off = 0x00
    if (phy_mode == MPPA_ETH_PCIE_PHY_MODE_PCIE_ILK) {
      lane_param.word             = __k1_io_read32((void*)&mppa_eth_pcie_csr[1]->lane_param.word);
      lane_param._.pcie_width     = 0x00; // pcie_width
      lane_param._.rx_encdet      = 0x00; // rx_encdet
      lane_param._.rx_sigdet      = 0x0F; // rx_sigdet
      lane_param._.lane_pwr_off   = 0x0F; // lane_pwr_off
      __k1_io_write32((void*)&mppa_eth_pcie_csr[1]->lane_param.word, lane_param.word);
    }
    lane_param.word             = __k1_io_read32((void*)&mppa_eth_pcie_csr[phy_id]->lane_param.word);
    lane_param._.pcie_width     = 0x00; // pcie_width
    lane_param._.rx_encdet      = 0x00; // rx_encdet
    lane_param._.rx_sigdet      = 0x0F; // rx_sigdet
    lane_param._.lane_pwr_off   = 0x0F; // lane_pwr_off
    __k1_io_write32((void*)&mppa_eth_pcie_csr[phy_id]->lane_param.word, lane_param.word);
  }

  // serdes_cfg = 0x00 (0x00 <=> 4 lanes Ethernet mode, 0x0F <=>  4 lanes interlaken mode)
  // init_ctlifc_pipe_compliant = 0x00
  // init_ctlifc_use_mgmt = 0x01
  switch (phy_mode) {
  case MPPA_ETH_PCIE_PHY_MODE_PCIE_ILK:
    serdes_ctrl.word            = __k1_io_read32((void*)&mppa_eth_pcie_csr[1]->serdes_ctrl.word);
    serdes_ctrl._.serdes_cfg    = 0x0F;     // serdes_cfg = Ilk mode
    __k1_io_write32((void*)&mppa_eth_pcie_csr[1]->serdes_ctrl.word, serdes_ctrl.word);
    serdes_ctrl.word            = __k1_io_read32((void*)&mppa_eth_pcie_csr[2]->serdes_ctrl.word);
    serdes_ctrl._.serdes_cfg    = 0x0F;     // serdes_cfg = Ilk mode
    __k1_io_write32((void*)&mppa_eth_pcie_csr[2]->serdes_ctrl.word, serdes_ctrl.word);
    break;
  case MPPA_ETH_PCIE_PHY_MODE_ETH_ILK:
    serdes_ctrl.word            = __k1_io_read32((void*)&mppa_eth_pcie_csr[0]->serdes_ctrl.word);
    serdes_ctrl._.serdes_cfg    = 0x0F;  // serdes_cfg = Ilk mode
    __k1_io_write32((void*)&mppa_eth_pcie_csr[0]->serdes_ctrl.word, serdes_ctrl.word);
    break;
  case MPPA_ETH_PCIE_PHY_MODE_ETH_40G:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_BASE_R:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_XAUI:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_RXAUI:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_1G:
    serdes_ctrl.word            = __k1_io_read32((void*)&mppa_eth_pcie_csr[0]->serdes_ctrl.word);
    serdes_ctrl._.serdes_cfg    = 0x00;  // serdes_cfg = Ethernet mode
    __k1_io_write32((void*)&mppa_eth_pcie_csr[0]->serdes_ctrl.word, serdes_ctrl.word);
    break;
  case MPPA_ETH_PCIE_PHY_MODE_PCIE:
    serdes_ctrl.word            = __k1_io_read32((void*)&mppa_eth_pcie_csr[1]->serdes_ctrl.word);
    serdes_ctrl._.serdes_cfg    = 0x00;     // serdes_cfg = PCIe mode
    __k1_io_write32((void*)&mppa_eth_pcie_csr[1]->serdes_ctrl.word, serdes_ctrl.word);
    serdes_ctrl.word            = __k1_io_read32((void*)&mppa_eth_pcie_csr[2]->serdes_ctrl.word);
    serdes_ctrl._.serdes_cfg    = 0x00;     // serdes_cfg = PCIe mode
    __k1_io_write32((void*)&mppa_eth_pcie_csr[2]->serdes_ctrl.word, serdes_ctrl.word);
    break;
  default:
    /* Re-enable interrupt */
    return 0x0;
    break;
  }
  if (phy_mode == MPPA_ETH_PCIE_PHY_MODE_PCIE_ILK) {
    serdes_ctrl.word                            = __k1_io_read32((void*)&mppa_eth_pcie_csr[1]->serdes_ctrl.word);
    serdes_ctrl._.init_ctlifc_pipe_compliant    = 0x00; // init_ctlifc_pipe_compliant
    serdes_ctrl._.init_ctlifc_use_mgmt          = 0x01; // init_ctlifc_use_mgmt
    __k1_io_write32((void*)&mppa_eth_pcie_csr[1]->serdes_ctrl.word, serdes_ctrl.word);
  }
  serdes_ctrl.word                            = __k1_io_read32((void*)&mppa_eth_pcie_csr[phy_id]->serdes_ctrl.word);
  serdes_ctrl._.init_ctlifc_pipe_compliant    = 0x00; // init_ctlifc_pipe_compliant
  serdes_ctrl._.init_ctlifc_use_mgmt          = 0x01; // init_ctlifc_use_mgmt
  __k1_io_write32((void*)&mppa_eth_pcie_csr[phy_id]->serdes_ctrl.word, serdes_ctrl.word);

  // PCS mode control : tx_mode & rx_mode
  // Per-lane divider to Serdes : tx_lane_div & rx_lane_div
  switch (phy_mode) {
  case MPPA_ETH_PCIE_PHY_MODE_PCIE_ILK:
    lane_mode.word          = __k1_io_read32((void*)&mppa_eth_pcie_csr[1]->lane_mode.word);
    lane_mode._.tx_mode     = 0x0B;     // tx_mode : 20 bit raw data : 01011
    lane_mode._.rx_mode     = 0x0B;     // rx_mode : 20 bit raw data : 01011
    lane_mode._.tx_lane_div = 0x00;     // tx_lane_div => pas de division de bande passante serdes
    lane_mode._.rx_lane_div = 0x00;     // rx_lane_div => pas de division de bande passante serdes
    __k1_io_write32((void*)&mppa_eth_pcie_csr[1]->lane_mode.word, lane_mode.word);
    lane_mode.word          = __k1_io_read32((void*)&mppa_eth_pcie_csr[2]->lane_mode.word);
    lane_mode._.tx_mode     = 0x0B;     // tx_mode : 20 bit raw data : 01011
    lane_mode._.rx_mode     = 0x0B;     // rx_mode : 20 bit raw data : 01011
    lane_mode._.tx_lane_div = 0x00;     // tx_lane_div => pas de division de bande passante serdes
    lane_mode._.rx_lane_div = 0x00;     // rx_lane_div => pas de division de bande passante serdes
    __k1_io_write32((void*)&mppa_eth_pcie_csr[2]->lane_mode.word, lane_mode.word);
    break;
  case MPPA_ETH_PCIE_PHY_MODE_ETH_ILK:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_40G:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_BASE_R:
    lane_mode.word          = __k1_io_read32((void*)&mppa_eth_pcie_csr[phy_id]->lane_mode.word);
    lane_mode._.tx_mode     = 0x0B; // tx_mode : 20 bit raw data : 01011
    lane_mode._.rx_mode     = 0x0B; // rx_mode : 20 bit raw data : 01011
                lane_mode._.tx_lane_div = 0x00; // tx_lane_div => pas de division de bande passante serdes
                lane_mode._.rx_lane_div = 0x00; // rx_lane_div => pas de division de bande passante serdes
                if(bw_div == 2) {
                  lane_mode._.tx_lane_div = 0x01; // tx_lane_div => pas de division de bande passante serdes
                  lane_mode._.rx_lane_div = 0x01; // rx_lane_div => pas de division de bande passante serdes
                }
                if(bw_div == 4) {
                  lane_mode._.tx_lane_div = 0x02; // tx_lane_div => pas de division de bande passante serdes
                  lane_mode._.rx_lane_div = 0x02; // rx_lane_div => pas de division de bande passante serdes
                }
                if(bw_div == 8) {
                  lane_mode._.tx_lane_div = 0x03; // tx_lane_div => pas de division de bande passante serdes
                  lane_mode._.rx_lane_div = 0x03; // rx_lane_div => pas de division de bande passante serdes
                }
    __k1_io_write32((void*)&mppa_eth_pcie_csr[phy_id]->lane_mode.word, lane_mode.word);
    break;
  case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_XAUI:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_RXAUI:
    lane_mode.word          = __k1_io_read32((void*)&mppa_eth_pcie_csr[phy_id]->lane_mode.word);
    lane_mode._.tx_mode     = 0x0B;     // tx_mode : XAUI 00010
    lane_mode._.rx_mode     = 0x0B;     // rx_mode : XAUI 00010
    lane_mode._.tx_lane_div = 0x01;     // tx_lane_div => pas de division de bande passante serdes
    lane_mode._.rx_lane_div = 0x01;     // rx_lane_div => pas de division de bande passante serdes
    __k1_io_write32((void*)&mppa_eth_pcie_csr[phy_id]->lane_mode.word, lane_mode.word);
    break;
  case MPPA_ETH_PCIE_PHY_MODE_ETH_1G:
    lane_mode.word          = __k1_io_read32((void*)&mppa_eth_pcie_csr[phy_id]->lane_mode.word);
    lane_mode._.tx_mode     = 0x09;     // tx_mode : 10 bit raw data 01001
    lane_mode._.rx_mode     = 0x09;     // rx_mode : 10 bit raw data 01001
    lane_mode._.tx_lane_div = 0x03;     // tx_lane_div => bande passante serdes / 8
    lane_mode._.rx_lane_div = 0x03;     // rx_lane_div => bande passante serdes / 8
    __k1_io_write32((void*)&mppa_eth_pcie_csr[phy_id]->lane_mode.word, lane_mode.word);
    break;
  default:
    return 0x00;
  }

  // pstate :
  // "11" : power off
  // "10" : logic off, idle mode
  // "01" : on, sleep mode
  // "00" : run
  // On active les lanes
  if (phy_mode == MPPA_ETH_PCIE_PHY_MODE_PCIE_ILK) {
    lane_ctrl.word              = __k1_io_read32((void*)&mppa_eth_pcie_csr[1]->lane_ctrl.word);
    lane_ctrl._.tx_pstate       = 0xaa; // tx_pstate : run
    lane_ctrl._.rx_pstate       = 0xaa; // rx_pstate : run
    lane_ctrl._.lane_x4_mode    = 0x00; // lane_x4_mode : off
    lane_ctrl._.lane_x2_mode    = 0x00; // lane_x2_mode : off
    __k1_io_write32((void*)&mppa_eth_pcie_csr[1]->lane_ctrl.word, lane_ctrl.word);
  }
  lane_ctrl.word              = __k1_io_read32((void*)&mppa_eth_pcie_csr[phy_id]->lane_ctrl.word);
  lane_ctrl._.tx_pstate       = 0xaa; // tx_pstate : run
  lane_ctrl._.rx_pstate       = 0xaa; // rx_pstate : run
  lane_ctrl._.lane_x4_mode    = 0x00; // lane_x4_mode : off
  lane_ctrl._.lane_x2_mode    = 0x00; // lane_x2_mode : off
  __k1_io_write32((void*)&mppa_eth_pcie_csr[phy_id]->lane_ctrl.word, lane_ctrl.word);

  /**********************************************************************************/
  // Select RC PLL
  /**********************************************************************************/
        mppa_eth_pcie_phy[phy_id]->glbl_pll_cfg_0.word = 0x0000374f;
        mppa_eth_pcie_phy[phy_id]->glbl_pll_cfg_2.word = 0x00000100;
        mppa_eth_pcie_phy[phy_id]->glbl_pll_cfg_3.word = 0x00002000;

  /**********************************************************************************/
  // Sortie de reset du PHY
  /**********************************************************************************/
  // On doit attendre 20 cycles avant de relacher le reset.
  __builtin_k1_barrier();
 #ifdef DEBUG_ETH_PLL
  printf("[ETH PLL] Releasing Ethernet PLL reset.\n");
 #endif
  if (phy_mode == MPPA_ETH_PCIE_PHY_MODE_PCIE_ILK) {
    __k1_io_write32((void*)&mppa_eth_pcie_csr[1]->rawmode_reset_n.word, 1);
    __k1_io_write32((void*)&mppa_eth_pcie_csr[2]->rawmode_reset_n.word, 1);
  } else {
    clear_ethernet_phy_serdes_reset();
  }

  mppa_eth_pcie_phy[phy_id]->glbl_misc_config_0._.resetn_ovrrd_val    = 0;
  mppa_eth_pcie_phy[phy_id]->glbl_misc_config_0._.resetn_ovrrd_en     = 1;
  if (phy_mode == MPPA_ETH_PCIE_PHY_MODE_PCIE_ILK) {
    mppa_eth_pcie_phy[1]->glbl_misc_config_0._.resetn_ovrrd_val = 0;
    mppa_eth_pcie_phy[1]->glbl_misc_config_0._.resetn_ovrrd_en  = 1;
  }

  mppa_eth_pcie_phy[phy_id]->glbl_cal_cfg._.pcs_sds_pll_vctrl_sel = 0xC;
  if (phy_mode == MPPA_ETH_PCIE_PHY_MODE_PCIE_ILK) {
    mppa_eth_pcie_phy[1]->glbl_cal_cfg._.pcs_sds_pll_vctrl_sel = 0xC;
  }

  mppa_eth_pcie_phy[phy_id]->glbl_misc_config_0._.resetn_ovrrd_en     = 0;
  mppa_eth_pcie_phy[phy_id]->glbl_misc_config_0._.resetn_ovrrd_val    = 1;
  if(phy_mode == MPPA_ETH_PCIE_PHY_MODE_PCIE_ILK) {
    mppa_eth_pcie_phy[1]->glbl_misc_config_0._.resetn_ovrrd_en  = 0;
    mppa_eth_pcie_phy[1]->glbl_misc_config_0._.resetn_ovrrd_val = 1;
  }
 #ifdef DEBUG_ETH_PLL
  printf("[ETH PLL] Waiting for Ethernet PLL to be locked.\n");
 #endif
  // On attend que le lock de la PLL
  while(__k1_io_read32((void*)&mppa_eth_pcie_csr[phy_id]->phystatus0.word) == 0);
  if (phy_mode == MPPA_ETH_PCIE_PHY_MODE_PCIE_ILK) {
    while(__k1_io_read32((void*)&mppa_eth_pcie_csr[1]->phystatus0.word) == 0);
  }
 #ifdef DEBUG_ETH_PLL
  printf("[ETH PLL] Ethernet PLL is locked.\n");
        if(phy_mode != MPPA_ETH_PCIE_PHY_MODE_PCIE_ILK) {
           if(mppa_eth_pcie_csr[phy_id]->pll1_ctrl._.pll_refclk_freq_sel == 0) {
             if(mppa_eth_pcie_csr[phy_id]->pll1_ctrl._.pll_hdr_sel == 0) {
               printf("[ETH PLL] VCO    frequency : %.2fMHz\n",  156.25 * (mppa_eth_pcie_csr[phy_id]->pll0_ctrl._.pll0_div + 1));
               printf("[ETH PLL] Serdes frequency : %.2fMHz\n", (156.25 * (mppa_eth_pcie_csr[phy_id]->pll0_ctrl._.pll0_div + 1)) / (mppa_eth_pcie_csr[phy_id]->pll0_ctrl._.pll0_pcs_div + 1));
               printf("[ETH PLL] Core   frequency : %.2fMHz\n", (156.25 * (mppa_eth_pcie_csr[phy_id]->pll0_ctrl._.pll0_div + 1)) / __k1_phy_get_mac_sds_core_div(phy_id));
             } else {
               printf("[ETH PLL] VCO    frequency : %.2fMHz\n",  156.25 * (mppa_eth_pcie_csr[phy_id]->pll0_ctrl._.pll0_div + 1));
               printf("[ETH PLL] Serdes frequency : %.2fMHz\n", (156.25 * (mppa_eth_pcie_csr[phy_id]->pll0_ctrl._.pll0_div + 1)) / (2 * (mppa_eth_pcie_csr[phy_id]->pll0_ctrl._.pll0_pcs_div + 1)));
               printf("[ETH PLL] Core   frequency : %.2fMHz\n", (156.25 * (mppa_eth_pcie_csr[phy_id]->pll0_ctrl._.pll0_div + 1)) / (2 * __k1_phy_get_mac_sds_core_div(phy_id)));
             }
           }
        }
 #endif

  // TC2 specific initialisation for decoupling
  if (__bsp_flavour == BSP_TC2) {
    for (i = 0; i < 4; i++) {
      mppa_eth_pcie_phy[0]->rx_cfg_0[i]._.pcs_sds_rx_tristate_en          = 0;
      mppa_eth_pcie_phy[0]->rx_cfg_4[i]._.pcs_sds_rx_terminate_to_vdda    = 0;
      mppa_eth_pcie_phy[0]->rx_cfg_5[i]._.pcs_sds_rx_pcie_mode_ovrrd_en   = 0;
      mppa_eth_pcie_phy[0]->rx_cfg_5[i]._.pcs_sds_rx_pcie_mode            = 0;
      mppa_eth_pcie_phy[0]->term_ctrl[i]._.rx_term100_ovrrd_en            = 1;
    }
  }

 #ifdef DEBUG_ETH_PLL
  printf("[ETH PLL] Configuring serdes.\n");
 #endif
  if(phy_mode == MPPA_ETH_PCIE_PHY_MODE_ETH_10G_BASE_R || phy_mode == MPPA_ETH_PCIE_PHY_MODE_ETH_40G) {
 #ifdef DEBUG_ETH_PLL
    printf("[ETH PLL] Updating serdes swing.\n");
 #endif
/*    for(i=0;i<4;i++) {
            if(phy_mode == MPPA_ETH_PCIE_PHY_MODE_ETH_10G_BASE_R) {
        mppa_eth_pcie_phy[0]->tx_cfg_0[i]._.cfg_tx_swing = 0x00;
        mppa_eth_pcie_phy[0]->tx_cfg_1[i]._.tx_swing_ovrrd_en = 1;
              mppa_eth_pcie_phy[0]->tx_preemph_0[i]._.cfg_tx_premptap = -1;
            } else {
        mppa_eth_pcie_phy[0]->tx_cfg_0[i]._.cfg_tx_swing = -1;
        mppa_eth_pcie_phy[0]->tx_cfg_1[i]._.tx_swing_ovrrd_en = 1;
              mppa_eth_pcie_phy[0]->tx_preemph_0[i]._.cfg_tx_premptap = 0;
          }
    }
*/

  }
  // on configure le PHY pour bit reverse
  for (i = 0; i < 4; i++) {
    if (phy_mode == MPPA_ETH_PCIE_PHY_MODE_PCIE_ILK) {
      pcs_misc_cfg_0.word             = __k1_io_read32((void*)&mppa_eth_pcie_phy[1]->pcs_misc_cfg_0[i].word);
      pcs_misc_cfg_0._.rx_bit_order   = 1;
      pcs_misc_cfg_0._.tx_bit_order   = 1;
      __k1_io_write32((void*)&mppa_eth_pcie_phy[1]->pcs_misc_cfg_0[i].word, pcs_misc_cfg_0.word);
    }
    pcs_misc_cfg_0.word             = __k1_io_read32((void*)&mppa_eth_pcie_phy[phy_id]->pcs_misc_cfg_0[i].word);
    pcs_misc_cfg_0._.rx_bit_order   = 1;
    pcs_misc_cfg_0._.tx_bit_order   = 1;
    __k1_io_write32((void*)&mppa_eth_pcie_phy[phy_id]->pcs_misc_cfg_0[i].word, pcs_misc_cfg_0.word);
  }

  // On attend que les signal d'acquittement retombent a 0 (signaux pcs_mac_*_ack == 0, visiblent dans phystatus1).
  while (__k1_io_read32((void*)&mppa_eth_pcie_csr[phy_id]->phystatus1.word) != 0) ;
  if (phy_mode == MPPA_ETH_PCIE_PHY_MODE_PCIE_ILK) {
    while (__k1_io_read32((void*)&mppa_eth_pcie_csr[1]->phystatus1.word) != 0) ;
  }
 #ifdef DEBUG_ETH_PLL
  printf("[ETH PLL] Ethernet PLL and serdes configuration done.\n");
 #endif


  return 0x01;
}

/**
 * \fn static __inline__ __k1_uint32_t __k1_phy_toggle_loopback( __k1_uint32_t phy_mode, int loopback_mode )
 * \brief Enable PHY loopback for Ethernet, PCIe or Interlaken
 * \param[in] phy_mode PHY mode to configure
 * \param[in] loopback_mode Loopback mode (0 or 1)
 * \return PHY loopback enabling status
 */
__k1_uint32_t
__k1_phy_toggle_loopback(__k1_uint32_t phy_mode, int loopback_mode)
{
  unsigned int i;
  unsigned int j;
  mppa_eth_pcie_phy_pma_loopback_ctrl_t pma_loopback_ctrl;
  mppa_eth_pcie_phy_pcs_misc_cfg_0_t pcs_misc_cfg_0;

  if ((loopback_mode < 0) || (loopback_mode > 3)) return 0;

  switch (phy_mode) {
  case MPPA_ETH_PCIE_PHY_MODE_ETH_ILK:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_40G:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_BASE_R:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_XAUI:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_RXAUI:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_1G:
    for(i=0;i<4;i++) {
       pma_loopback_ctrl.word = __k1_io_read32((void*)&mppa_eth_pcie_phy[0]->pma_loopback_ctrl[i].word);
       pma_loopback_ctrl._.cfg_ln_loopback_mode = loopback_mode & 1;
       __k1_io_write32((void*)&mppa_eth_pcie_phy[0]->pma_loopback_ctrl[i].word, pma_loopback_ctrl.word);
       pcs_misc_cfg_0.word = __k1_io_read32((void*)&mppa_eth_pcie_phy[0]->pcs_misc_cfg_0[i].word);
       pcs_misc_cfg_0._.cfg_pcs_loopback = (loopback_mode >> 1) & 1;
       __k1_io_write32((void*)&mppa_eth_pcie_phy[0]->pcs_misc_cfg_0[i].word, pcs_misc_cfg_0.word);
    }
    break;
  case MPPA_ETH_PCIE_PHY_MODE_PCIE_ILK:
  case MPPA_ETH_PCIE_PHY_MODE_PCIE:
    for(i=0;i<4;i++) {
      for(j=1;j<3;j++) {
        pma_loopback_ctrl.word = __k1_io_read32((void*)&mppa_eth_pcie_phy[j]->pma_loopback_ctrl[i].word);
        pma_loopback_ctrl._.cfg_ln_loopback_mode = loopback_mode & 1;
        __k1_io_write32((void*)&mppa_eth_pcie_phy[j]->pma_loopback_ctrl[i].word, pma_loopback_ctrl.word);
        pcs_misc_cfg_0.word = __k1_io_read32((void*)&mppa_eth_pcie_phy[j]->pcs_misc_cfg_0[i].word);
        pcs_misc_cfg_0._.cfg_pcs_loopback = (loopback_mode >> 1) & 1;
        __k1_io_write32((void*)&mppa_eth_pcie_phy[j]->pcs_misc_cfg_0[i].word, pcs_misc_cfg_0.word);
      }
    }
    break;
  default:
    // Unknow mode.
    return 0x00;
  }
  return 0x01;
}

__k1_uint32_t
__k1_phy_polarity_reverse(__k1_uint8_t phy_mode, __k1_uint8_t rx_lane_valid, __k1_uint8_t tx_lane_valid)
{
  int i;
  mppa_eth_pcie_phy_pcs_misc_cfg_1_t pcs_misc_cfg_1;

  switch (phy_mode) {
  case MPPA_ETH_PCIE_PHY_MODE_ETH_ILK:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_40G:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_BASE_R:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_XAUI:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_RXAUI:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_1G:
    for (i = 0; i < 4; i++) {
      pcs_misc_cfg_1.word = __k1_io_read32((void*)&mppa_eth_pcie_phy[0]->pcs_misc_cfg_1[i].word);
      if (((tx_lane_valid >> i) & 1) != 0) {
        pcs_misc_cfg_1._.tx_polarity = 1;
      }
      if (((rx_lane_valid >> i) & 1) != 0) {
        pcs_misc_cfg_1._.rx_polarity_ovrrd_val  = 1;
        pcs_misc_cfg_1._.rx_polarity_ovrrd_en   = 1;
      }
      __k1_io_write32((void*)&mppa_eth_pcie_phy[0]->pcs_misc_cfg_1[i].word, pcs_misc_cfg_1.word);
    }
    break;
  case MPPA_ETH_PCIE_PHY_MODE_PCIE_ILK:
  case MPPA_ETH_PCIE_PHY_MODE_PCIE:
    for (i = 0; i < 4; i++) {
      pcs_misc_cfg_1.word = __k1_io_read32((void*)&mppa_eth_pcie_phy[2]->pcs_misc_cfg_1[i].word);
      if (((tx_lane_valid >> i) & 1) != 0) {
        pcs_misc_cfg_1._.tx_polarity = 1;
      }
      if (((rx_lane_valid >> i) & 1) != 0) {
        pcs_misc_cfg_1._.rx_polarity_ovrrd_val  = 1;
        pcs_misc_cfg_1._.rx_polarity_ovrrd_en   = 1;
      }
      __k1_io_write32((void*)&mppa_eth_pcie_phy[2]->pcs_misc_cfg_1[i].word, pcs_misc_cfg_1.word);

      pcs_misc_cfg_1.word = __k1_io_read32((void*)&mppa_eth_pcie_phy[1]->pcs_misc_cfg_1[i].word);
      if (((tx_lane_valid >> (4 + i)) & 1) != 0) {
        pcs_misc_cfg_1._.tx_polarity = 1;
      }
      if (((rx_lane_valid >> (4 + i)) & 1) != 0) {
        pcs_misc_cfg_1._.rx_polarity_ovrrd_val  = 1;
        pcs_misc_cfg_1._.rx_polarity_ovrrd_en   = 1;
      }
      __k1_io_write32((void*)&mppa_eth_pcie_phy[1]->pcs_misc_cfg_1[i].word, pcs_misc_cfg_1.word);
    }
    break;
  default:
    // Unknow mode.
    return 0x00;
    break;
  }
  return 0x01;
}

__k1_uint32_t
__k1_phy_monitor_core_clk_on_dmon(__k1_uint8_t phy_mode)
{
  int index;

  switch (phy_mode) {
  case MPPA_ETH_PCIE_PHY_MODE_ETH_ILK:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_40G:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_BASE_R:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_XAUI:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_10G_RXAUI:
  case MPPA_ETH_PCIE_PHY_MODE_ETH_1G:
    index = 0;
    break;
  default:
    index = 1;
    break;
  }
  mppa_eth_pcie_phy[index]->glbl_tad.word                     = 0x00000012;
  mppa_eth_pcie_phy[index]->glbl_tm_admon._.pcs_sds_tm_admon  = 0x40;

  return 0x01;
}
