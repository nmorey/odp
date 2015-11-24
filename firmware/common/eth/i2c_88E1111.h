#ifndef __HAL_ETH_I2C_H__
#define __HAL_ETH_I2C_H__

#include <HAL/hal/hal.h>
#include <stdint.h>

#define UNUSED(x) (void)(x)

#ifdef __k1io__
int mppa_i2c_init(volatile mppa_i2c_master_t * master, int i2c_bus,
		  int i2c_coma_pin, int i2c_reset_n_pin, int i2c_int_n_pin, int i2c_gic);
int mppa_i2c_register_read(volatile void *context, uint8_t chip_id, uint8_t reg, uint16_t * val);
int mppa_i2c_register_write(volatile void *context, uint8_t chip_id, uint8_t reg, uint16_t val);
unsigned int mppa_eth_i2c_get_status(int phy);
#endif

#endif
