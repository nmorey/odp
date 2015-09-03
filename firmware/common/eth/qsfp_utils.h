#ifndef QSFP_DUMP_H
#define QSFP_DUMP_H 
#include "bsp_i2c.h"
#include <unistd.h>
#include <stdint.h>
int qsfp_read(volatile mppa_i2c_master_t* i2c_master, uint8_t reg, uint8_t* buf, int len);
int qsfp_select_page(volatile mppa_i2c_master_t* i2c_master, uint8_t page);
int qsfp_write_reg(volatile mppa_i2c_master_t* i2c_master, uint8_t reg, uint8_t val);
int qsfp_dump_registers(volatile mppa_i2c_master_t* i2c_master);
int qsfp_write(volatile mppa_i2c_master_t* i2c_master, uint8_t reg, uint8_t* buf, int len);
#endif /* QSFP_DUMP_H */
