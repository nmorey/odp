#include "bsp_i2c.h"
#include "i2c_88E1111.h"
#include <unistd.h>
#ifdef VERBOSE
#include <stdio.h>
#endif

int mppa_i2c_init(volatile mppa_i2c_master_t* master, int i2c_bus, int i2c_coma_pin, int i2c_reset_n_pin, int i2c_int_n_pin, int i2c_gic)
{
  UNUSED(master);
  uint32_t gpio_mask;
  uint32_t dir_mask;
  int periph = i2c_bus & 0x1;
  volatile mppa_gpio_t* gpio;

  gpio = mppa_gpio_get_base(periph);
  gpio_mask = mppa_gpio_get_enabled_mask(gpio);
  /* Activate GPIO for Reset_n/COMA/int_n signals */
#ifdef VERBOSE
  printf("[I2C] Previous mask for GPIO[%d..%d]=0x%.8x\n", periph*32, periph*32+31, (unsigned int)gpio_mask);
#endif
  gpio_mask |= 1 << i2c_reset_n_pin; // reset_n_pin
  gpio_mask |= 1 << i2c_coma_pin;    // coma_pin
  gpio_mask |= 1 << i2c_int_n_pin;   // int_n_pin
#ifdef VERBOSE
  printf("[I2C] New mask for GPIO[%d..%d]=0x%.8x\n", periph*32, periph*32+31, (unsigned int)gpio_mask);
#endif
  mppa_gpio_enable_mask(gpio, gpio_mask);

  /* Output pin for Reset_n/COMA signals */
  dir_mask =mppa_gpio_get_direction_mask(gpio);
  dir_mask |=   1 << i2c_reset_n_pin; // reset_n_pin
  dir_mask |=   1 << i2c_coma_pin;    // coma_pin
  dir_mask &= ~(1 << i2c_int_n_pin);  // int_n_pin
#ifdef VERBOSE
  printf("[I2C] Output direction for GPIO[%d..%d]=0x%.8x\n", periph*32, periph*32+31, (unsigned int)dir_mask);
#endif
  mppa_gpio_set_direction_output_mask(gpio, dir_mask);

  /* Generate a reset sequence */
#ifdef VERBOSE
  printf("[I2C] Generate reset sequence\n");
#endif
  mppa_gpio_write(gpio, i2c_coma_pin, 0); /* set power mode to off */
  mppa_gpio_write(gpio, i2c_reset_n_pin, 0); /* reset the Ethernet transceiver*/
  mppa_gpio_write(gpio, i2c_reset_n_pin, 1); /* unreset the Ethernet transceiver*/

  /* Input pin for Interrupt Generation */
  dir_mask = ~(mppa_gpio_get_direction_mask(gpio));
  dir_mask |= 1 << i2c_gic;
#ifdef VERBOSE
  printf("[I2C] Input direction for GPIO[%d..%d]=0x%.8x\n", periph*32, periph*32+31, (unsigned int)dir_mask);
#endif
  mppa_gpio_set_direction_input_mask(gpio, dir_mask);
#ifdef VERBOSE
  printf("[I2C] Set GPIO14 for Interrupt 0 sensitivity to low level\n");
#endif	
  mppa_gpio_set_sensitivity(gpio, i2c_gic, MPPA_GPIO_SENSITIVITY_LEVEL_LOW);
  mppa_gpio_enable_interrupt(gpio, i2c_gic, i2c_int_n_pin);

  return 0;
}

int mppa_i2c_register_read(volatile void* context, uint8_t chip_id, uint8_t reg, uint16_t* val)
{
  uint8_t value[2];
  int status;
  volatile mppa_i2c_master_t* master = (volatile mppa_i2c_master_t*)context;
  status = mppa_i2c_master_write((void*)master,chip_id,&reg,1);
  if(status != 0){
	  return status;
  }
  while(!mppa_i2c_master_idle(master)){ usleep(WAITING_TIME);  };

  status = mppa_i2c_master_read((void*)master, chip_id,value,2);
  if(status != 0) {
	  return status;
  }
  while (!mppa_i2c_master_idle(master)){ usleep(WAITING_TIME); };
  *val = ((((uint16_t)(value[0]))<<8)|(((uint16_t)(value[1]))));
  return status;

}

int mppa_i2c_register_write(volatile void* context, uint8_t chip_id, uint8_t reg, uint16_t val)
{
  uint8_t value[3]={((reg)&0xFF), ((val>>8)&0xFF),(val&0xFF)};
  int status =  mppa_i2c_master_write((void*)context,chip_id,value,3);
  volatile mppa_i2c_master_t* master = (volatile mppa_i2c_master_t*)context;
  while (!mppa_i2c_master_idle(master)){ usleep(WAITING_TIME); };
  return status;
}
