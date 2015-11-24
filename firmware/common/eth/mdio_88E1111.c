#include "mdio_88E1111.h"
#include <HAL/hal/hal.h>
#ifdef VERBOSE
#include <stdio.h>
#else
#ifdef MDIO_DEBUG
#include <stdio.h>
#endif
#endif

#include "mppa_ethernet_private.h"
#include "mppa_ethernet_shared.h"

#define MDC_GPIO_PIN  62
#define MDIO_GPIO_PIN 63

#define MDC_GPIO_DEVICE  (MDC_GPIO_PIN  > 31 ? 1 : 0)
#define MDIO_GPIO_DEVICE (MDIO_GPIO_PIN > 31 ? 1 : 0)

#define MDC_GPIO_NUMBER  (MDC_GPIO_PIN  % 32)
#define MDIO_GPIO_NUMBER (MDIO_GPIO_PIN % 32)

#define MDIO_C45       (1<<15)
#define MDIO_C45_ADDR  (MDIO_C45 | 0)
#define MDIO_C45_READ  (MDIO_C45 | 3)
#define MDIO_C45_WRITE (MDIO_C45 | 1)

#define MDIO_SETUP_TIME  10
#define MDIO_HOLD_TIME   10
#define MDIO_PERIOD      400000	// Delay in ps
#define MDIO_HALF_PERIOD (MDIO_PERIOD >> 1)
#define MDIO_READ_DELAY  300000	// Delay in ps

#define MII_COMMAND_READ  2
#define MII_COMMAND_WRITE 1


static int get_platform_half_period_delay(int phy __attribute__ ((unused)))
{
	return MDIO_HALF_PERIOD / 50000;
}

static int get_platform_read_cycle_delay(int phy __attribute__ ((unused)))
{
	return MDIO_READ_DELAY / 50000;
}

static inline void mppa_eth_set_mdc(int level)
{
#ifdef MDIO_DEBUG
	printf("[MDIO DEBUG] Call to mppa_eth_set_mdc(%u).\n", level);
#endif
	mppa_gpio_write(mppa_gpio[MDC_GPIO_DEVICE], MDC_GPIO_NUMBER, level);
#ifdef MDIO_DEBUG
	printf("[MDIO DEBUG] Return from mppa_eth_set_mdc(%u).\n", level);
#endif
}

static inline void mppa_eth_set_mdio_dir(int output)
{
#ifdef MDIO_DEBUG
	printf("[MDIO DEBUG] Call to mppa_eth_set_mdio_dir(%u).\n", output);
#endif
	if (output != 0) {
		mppa_gpio_set_direction_output(mppa_gpio[MDIO_GPIO_DEVICE], MDIO_GPIO_NUMBER);
	} else {
		mppa_gpio_set_direction_input(mppa_gpio[MDIO_GPIO_DEVICE], MDIO_GPIO_NUMBER);
	}
}

static inline void mppa_eth_set_mdio_data(int value)
{
#ifdef MDIO_DEBUG
	printf
	    ("[MDIO DEBUG] Call to mppa_eth_set_mdio_data  mppa_gpio[MDIO_GPIO_DEVICE] = 0x%x (0x%.8x).\n",
	     mppa_gpio[MDIO_GPIO_DEVICE], value);
#endif
	mppa_gpio_write(mppa_gpio[MDIO_GPIO_DEVICE], MDIO_GPIO_NUMBER, value);
#ifdef MDIO_DEBUG
	printf("[MDIO DEBUG] Return from mppa_eth_set_mdio_data(0x%.8x).\n", value);
#endif
}

static inline int mppa_eth_get_mdio_data(void)
{
#ifdef MDIO_DEBUG
	printf("[MDIO DEBUG] Call to mppa_eth_get_mdio_data().\n");
#endif
	return mppa_gpio_read(mppa_gpio[MDIO_GPIO_DEVICE], MDIO_GPIO_NUMBER);
}

int mppa_eth_mdio_init(int phy)
{
	UNUSED(phy);
	unsigned int mppa_clk_period_ps;
	/* streaming should be enabled here */
	mppa_clk_period_ps = 50000;	/* FPGA, 20 MHz, should be 40... */
#ifdef VERBOSE
	printf("[MDIO] Initializing MDIO interface.\n");
#endif
	switch (mppa_clk_period_ps) {
	case 2500:
		// 400 MHz
		//mppa_eth_mdio_delay_context->half_period_cycle_delay = MDIO_HALF_PERIOD / 2500;
		//mppa_eth_mdio_delay_context->read_cycle_delay        = MDIO_READ_DELAY  / 2500;
		break;
	case 5000:
		// 200 MHz
		//mppa_eth_mdio_delay_context->half_period_cycle_delay = MDIO_HALF_PERIOD / 5000;
		//mppa_eth_mdio_delay_context->read_cycle_delay        = MDIO_READ_DELAY  / 5000;
		break;
	case 10000:
		// 100 MHz
		//mppa_eth_mdio_delay_context->half_period_cycle_delay = MDIO_HALF_PERIOD / 10000;
		//mppa_eth_mdio_delay_context->read_cycle_delay        = MDIO_READ_DELAY  / 10000;
		break;
	case 50000:
		// 20 MHz - FPGA
		//mppa_eth_mdio_delay_context->half_period_cycle_delay = MDIO_HALF_PERIOD / 50000;
		//mppa_eth_mdio_delay_context->read_cycle_delay        = MDIO_READ_DELAY  / 50000;
		// GPIO Init
		mppa_gpio_reset(mppa_gpio[0]);
		mppa_gpio_reset(mppa_gpio[1]);
		mppa_sysctl_disable_dna();
		mppa_sysctl_enable_gpio_mask(mppa_sysctl[0], 0xffffffff);	/* all pads to GPIO mode */
		mppa_sysctl_enable_gpio_mask(mppa_sysctl[1], 0xffffffff);	/* all pads to GPIO mode */
		mppa_gpio_set_direction_input_mask(mppa_gpio[1], 1 << 29);
		mppa_gpio_set_direction_output_mask(mppa_gpio[1], 3 << 30);
		break;
	default:
		return -1;
		break;
	}
	mppa_eth_set_mdio_dir(1);
	mppa_eth_set_mdio_data(0);
	mppa_eth_set_mdc(0);
	return 0;
}

int mppa_eth_mdio_finish(int phy)
{
	UNUSED(phy);
	return 0;
}

void mppa_eth_mdio_send_bit(int phy, int val)
{
#ifdef MDIO_DEBUG
	printf("[MDIO DEBUG] Call to mppa_eth_mdio_send_bit(%d, %u).\n", phy, val);
#endif
	mppa_eth_set_mdio_data(val);
	__k1_cpu_backoff(get_platform_half_period_delay(phy));
	mppa_eth_set_mdc(1);
	__k1_cpu_backoff(get_platform_half_period_delay(phy));
	mppa_eth_set_mdc(0);
#ifdef MDIO_DEBUG
	printf("[MDIO DEBUG] Return from mppa_eth_mdio_send_bit(%d, %u).\n", phy, val);
#endif
}

int mppa_eth_mdio_get_bit(int phy)
{
#ifdef MDIO_DEBUG
	printf("[MDIO DEBUG] Call to mppa_eth_mdio_get_bit(%d).\n", phy);
#endif
	__k1_cpu_backoff(get_platform_half_period_delay(phy));
	mppa_eth_set_mdc(1);
	__k1_cpu_backoff(get_platform_read_cycle_delay(phy));
	mppa_eth_set_mdc(0);
	return mppa_eth_get_mdio_data();
}

void mppa_eth_mdio_send_num(int phy, uint8_t val, int bits)
{
#ifdef MDIO_DEBUG
	printf("[MDIO DEBUG] Call to mppa_eth_mdio_send_num(%d, %u, %u).\n", phy, val, bits);
#endif
	int i = bits;
	while (i != 0) {
		i--;
		mppa_eth_mdio_send_bit(phy, (val >> i) & 1);
	}
}

unsigned mppa_eth_mdio_get_num(int phy, int bits)
{
#ifdef MDIO_DEBUG
	printf("[MDIO DEBUG] Call to mppa_eth_mdio_get_num(%d, %u).\n", phy, bits);
#endif
	unsigned ret = 0;
	int i;
	for (i = bits - 1; i >= 0; i--) {
		ret <<= 1;
		ret |= mppa_eth_mdio_get_bit(phy);
	}
	return ret;
}

void mppa_eth_mdio_cmd(int phy, int op, uint8_t chip_id, uint8_t reg)
{
#ifdef MDIO_DEBUG
	printf("[MDIO DEBUG] Call to mppa_eth_mdio_cmd(%d, %u, %u, %u).\n", phy, op, chip_id, reg);
#endif
	int i;
	mppa_eth_set_mdio_dir(1);
	/* preamble of 32+1 '1's */
	for (i = 0; i < 32; i++) {
		mppa_eth_mdio_send_bit(phy, 1);
	}
	/* start bit 01 */
	mppa_eth_mdio_send_bit(phy, 0);
	mppa_eth_mdio_send_bit(phy, 1);
	/* opcode read 10 or write 01 */
	mppa_eth_mdio_send_bit(phy, (op >> 1) & 1);
	mppa_eth_mdio_send_bit(phy, op & 1);

	mppa_eth_mdio_send_num(phy, chip_id, 5);
	mppa_eth_mdio_send_num(phy, reg, 5);
}

/* Read register of a PHY
 * chip_id number to access
 * unsigned reg register address
 * uint32_t *val ptr to read buffer
 */
int mppa_eth_mdio_read(volatile int phy, uint8_t chip_id, uint8_t reg, uint16_t * val)
{
#ifdef MDIO_DEBUG
	printf
	    ("[MDIO DEBUG] Call to mppa_eth_mdio_read(%d, %u, %u, 0x%.8x).\n",
	     phy, chip_id, reg, val);
#endif

	mppa_eth_mdio_cmd(phy, MII_COMMAND_READ, chip_id, reg);

	mppa_eth_set_mdio_dir(0);
	__k1_cpu_backoff(get_platform_half_period_delay(phy));
	mppa_eth_set_mdc(1);
	__k1_cpu_backoff(get_platform_half_period_delay(phy));
	mppa_eth_set_mdc(0);

	*val = mppa_eth_mdio_get_num(phy, 16);
	mppa_eth_mdio_get_bit(phy);
	return 0;
}

/* Read register of a PHY
 * chip_id number to access
 * unsigned reg register address
 * uint32_t val value to write
 * */
int mppa_eth_mdio_write(volatile int phy, uint8_t chip_id, uint8_t reg, uint16_t val)
{
#ifdef MDIO_DEBUG
	printf
	    ("[MDIO DEBUG] Call to mppa_eth_mdio_write(%d, %u, %u, 0x%.8x).\n",
	     phy, chip_id, reg, val);
#endif

	mppa_eth_mdio_cmd(phy, MII_COMMAND_WRITE, chip_id, reg);

	/* send turnaround 10 */
	mppa_eth_mdio_send_bit(phy, 1);
	mppa_eth_mdio_send_bit(phy, 0);

	mppa_eth_mdio_send_num(phy, val, 16);
	mppa_eth_set_mdio_dir(0);
	mppa_eth_mdio_get_bit(phy);
	return 0;
}
