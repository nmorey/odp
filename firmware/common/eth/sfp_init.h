#include <stdio.h>
#include <HAL/hal/hal.h>


#define GPIO_ID_10GBE_0_TX_FAULT   0
#define GPIO_ID_10GBE_0_TX_DISABLE 1
#define GPIO_ID_10GBE_0_MOD_ABS    2
#define GPIO_ID_10GBE_0_RX_LOS     3
#define GPIO_ID_10GBE_0_RS0        4
#define GPIO_ID_10GBE_0_RS1        5
#define GPIO_ID_10GBE_1_TX_FAULT   6
#define GPIO_ID_10GBE_1_TX_DISABLE 7
#define GPIO_ID_10GBE_1_MOD_ABS    8
#define GPIO_ID_10GBE_1_RX_LOS     9
#define GPIO_ID_10GBE_1_RS0        10
#define GPIO_ID_10GBE_1_RS1        11

static __inline__ void eth_sfp_print_status(void)
{
  volatile mppa_gpio_t* gpio;
  gpio = mppa_gpio_get_base(0);
  printf("Eth SFP[0] TX_FAULT : %u\n", mppa_gpio_read(gpio, GPIO_ID_10GBE_0_TX_FAULT));
  printf("Eth SFP[0] MOD_ABS  : %u\n", mppa_gpio_read(gpio, GPIO_ID_10GBE_0_MOD_ABS));
  printf("Eth SFP[0] RX_LOS   : %u\n", mppa_gpio_read(gpio, GPIO_ID_10GBE_0_RX_LOS));
  printf("Eth SFP[1] TX_FAULT : %u\n", mppa_gpio_read(gpio, GPIO_ID_10GBE_1_TX_FAULT));
  printf("Eth SFP[1] MOD_ABS  : %u\n", mppa_gpio_read(gpio, GPIO_ID_10GBE_1_MOD_ABS));
  printf("Eth SFP[1] RX_LOS   : %u\n", mppa_gpio_read(gpio, GPIO_ID_10GBE_1_RX_LOS));
}

static __inline__ void eth_sfp_tx_off(void)
{
  volatile mppa_gpio_t* gpio;
  gpio = mppa_gpio_get_base(0);
  mppa_gpio_write(gpio, GPIO_ID_10GBE_0_TX_DISABLE, 1);
  mppa_gpio_write(gpio, GPIO_ID_10GBE_1_TX_DISABLE, 1);
}

static __inline__ void eth_sfp_tx_on(void)
{
  volatile mppa_gpio_t* gpio;
  gpio = mppa_gpio_get_base(0);
  mppa_gpio_write(gpio, GPIO_ID_10GBE_0_TX_DISABLE, 0);
  mppa_gpio_write(gpio, GPIO_ID_10GBE_1_TX_DISABLE, 0);
}

int sfp_init(void)
{
  __k1_uint32_t gpio_mask;
  __k1_uint32_t dir_mask;
  volatile mppa_gpio_t* gpio;

  // Set to "11" RS pin, configure TX_FAULT, MOD_ABS and RX_LOS in read mode, set to '0' TX_DISABLE
#ifdef VERBOSE
  printf("Configuring SFP+ GPIO pin\n");
#endif
  gpio      = mppa_gpio_get_base(0);
  gpio_mask = mppa_gpio_get_enabled_mask(gpio);
  gpio_mask &= ~(1 << GPIO_ID_10GBE_0_TX_FAULT);
  gpio_mask &= ~(1 << GPIO_ID_10GBE_0_TX_DISABLE);
  gpio_mask &= ~(1 << GPIO_ID_10GBE_0_MOD_ABS);
  gpio_mask &= ~(1 << GPIO_ID_10GBE_0_RX_LOS);
  gpio_mask &= ~(1 << GPIO_ID_10GBE_0_RS0);
  gpio_mask &= ~(1 << GPIO_ID_10GBE_0_RS1);
  gpio_mask &= ~(1 << GPIO_ID_10GBE_1_TX_FAULT);
  gpio_mask &= ~(1 << GPIO_ID_10GBE_1_TX_DISABLE);
  gpio_mask &= ~(1 << GPIO_ID_10GBE_1_MOD_ABS);
  gpio_mask &= ~(1 << GPIO_ID_10GBE_1_RX_LOS);
  gpio_mask &= ~(1 << GPIO_ID_10GBE_1_RS0);
  gpio_mask &= ~(1 << GPIO_ID_10GBE_1_RS1);
  mppa_gpio_enable_mask(gpio, gpio_mask);

  dir_mask = mppa_gpio_get_direction_mask(gpio);
  dir_mask |= 1 << GPIO_ID_10GBE_0_TX_DISABLE;
  dir_mask |= 1 << GPIO_ID_10GBE_0_RS0;
  dir_mask |= 1 << GPIO_ID_10GBE_0_RS1;
  dir_mask |= 1 << GPIO_ID_10GBE_1_TX_DISABLE;
  dir_mask |= 1 << GPIO_ID_10GBE_1_RS0;
  dir_mask |= 1 << GPIO_ID_10GBE_1_RS1;
  mppa_gpio_set_direction_output_mask(gpio, dir_mask);

  mppa_gpio_write(gpio, GPIO_ID_10GBE_0_TX_DISABLE, 0);
  mppa_gpio_write(gpio, GPIO_ID_10GBE_0_RS0, 1);
  mppa_gpio_write(gpio, GPIO_ID_10GBE_0_RS1, 1);
  mppa_gpio_write(gpio, GPIO_ID_10GBE_1_TX_DISABLE, 0);
  mppa_gpio_write(gpio, GPIO_ID_10GBE_1_RS0, 1);
  mppa_gpio_write(gpio, GPIO_ID_10GBE_1_RS1, 1);

  dir_mask = ~(mppa_gpio_get_direction_mask(gpio));
  dir_mask |= 1 << GPIO_ID_10GBE_0_TX_FAULT;
  dir_mask |= 1 << GPIO_ID_10GBE_0_MOD_ABS;
  dir_mask |= 1 << GPIO_ID_10GBE_0_RX_LOS;
  dir_mask |= 1 << GPIO_ID_10GBE_1_TX_FAULT;
  dir_mask |= 1 << GPIO_ID_10GBE_1_MOD_ABS;
  dir_mask |= 1 << GPIO_ID_10GBE_1_RX_LOS;
  mppa_gpio_set_direction_input_mask(gpio, dir_mask);

#ifdef VERBOSE
  eth_sfp_print_status();
#endif
  return 0;
}
