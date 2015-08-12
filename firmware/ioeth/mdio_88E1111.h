#ifndef __HAL_ETH_MDIO_H__
#define __HAL_ETH_MDIO_H__

#include <stdint.h>
#define UNUSED(x) (void)(x)

typedef struct mppa_eth_mdio_delay_context {
  int streaming_mode;
  unsigned int mppa_clk_period_ps;
  unsigned int half_period_cycle_delay;
  unsigned int read_cycle_delay;
} mppa_eth_mdio_delay_context_t;

int mppa_eth_mdio_init(int phy);
int mppa_eth_mdio_finish(int phy);
int mppa_eth_mdio_read(volatile int phy, uint8_t chip_id, uint8_t reg, uint16_t *val);
int mppa_eth_mdio_write(volatile int phy, uint8_t chip_id, uint8_t reg, uint16_t val);

#endif
