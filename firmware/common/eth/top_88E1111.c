#include "top_88E1111.h"
#include <bsp_phy.h>
#include <bsp_i2c.h>
#ifdef VERBOSE
#include <stdio.h>
#endif

#define I2C_BITRATE      100000
#define I2C_1GE_MASTERID 1
#define I2C_1GE_ACKVAL   0

#define CHIP_88E1111_LED_ON    3
#define CHIP_88E1111_LED_OFF   2
#define CHIP_88E1111_LED_BLINK 1
#define CHIP_88E1111_LED_AUTO  0

int mppa_88E1111_get_mode(mppa_88E1111_interface_t * interface, uint8_t * mode)
{
	int status = 0;
	uint16_t reg;

	// Read bit 3:0 of register 27, whatever the page is.
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 27, &reg);
	*mode = ((uint8_t) (reg)) & 0x0f;
#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Mode is : ", interface->chip_id);
	switch (*mode) {
	case 0:
		printf("SGMII with Clock with SGMII Auto-Neg to copper.\n");
		break;
	case 3:
		printf("RGMII to Fiber.\n");
		break;
	case 4:
		printf("SGMII without Clock with SGMII Auto-Neg to copper.\n");
		break;
	case 6:
		printf("RGMII to SGMII.\n");
		break;
	case 7:
		printf("GMII to Fiber.\n");
		break;
	case 8:
		printf("1000BASE-X without Clock with 1000BASE-X Auto-Neg to copper (GBIC).\n");
		break;
	case 9:
		printf("RTBI to Copper.\n");
		break;
	case 11:
		printf("RGMII/Modified MII to Copper.\n");
		break;
	case 12:
		printf("1000BASE-X without Clock without 1000BASE-X Auto-Neg to copper (GBIC).\n");
		break;
	case 13:
		printf("TBI to Copper.\n");
		break;
	case 14:
		printf("GMII to SGMII.\n");
		break;
	case 15:
		printf("GMII to Copper.\n");
		break;
	default:
		printf("Reserved.\n");
		break;
	}
#endif
	return status;
}

int mppa_88E1111_set_mode(mppa_88E1111_interface_t * interface, uint8_t mode)
{
	int status = 0;
	uint16_t reg;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Updating mode to 0x%.2x\n", interface->chip_id, mode);
#endif
	// Set bit 3:0 of register 27, whatever the page is.
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 27, &reg);
	reg = (reg & 0xfff0) | (((uint16_t) (mode)) & 0x000f);
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 27, reg);
	status |= mppa_88E1111_copper_reset(interface);
	status |= mppa_88E1111_fiber_reset(interface);
	return status;
}

int mppa_88E1111_fiber_get_duplex_mode(mppa_88E1111_interface_t * interface, uint8_t * duplex)
{
	int status = 0;
	uint16_t reg;

	// Select page 1
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = (reg & 0xff00) | 0x0001;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Get bit 8 of register 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 0, &reg);
	*duplex = (reg >> 8) & 0x0001;
#ifdef VERBOSE
	if (*duplex == 0) {
		printf
		    ("[88E1111 0x%.2x] Fiber is working in half duplex mode.\n",
		     interface->chip_id);
	} else {
		printf
		    ("[88E1111 0x%.2x] Fiber is working in full duplex mode.\n",
		     interface->chip_id);
	}
#endif
	return status;
}

int mppa_88E1111_copper_get_duplex_mode(mppa_88E1111_interface_t * interface, uint8_t * duplex)
{
	int status = 0;
	uint16_t reg;

	// Select page 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = reg & 0xff00;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Get bit 8 of register 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 0, &reg);
	*duplex = (reg >> 8) & 0x0001;
#ifdef VERBOSE
	if (*duplex == 0) {
		printf
		    ("[88E1111 0x%.2x] Copper is working in half duplex mode.\n",
		     interface->chip_id);
	} else {
		printf
		    ("[88E1111 0x%.2x] Copper is working in full duplex mode.\n",
		     interface->chip_id);
	}
#endif
	return status;
}

int mppa_88E1111_copper_full_duplex_enable(mppa_88E1111_interface_t * interface)
{
	int status = 0;
	uint16_t reg;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Activating copper full duplex mode.\n", interface->chip_id);
#endif
	//  status |= mppa_88E1111_copper_autoneg_disable(interface);
	// Select page 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = reg & 0xff00;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Set bit 8 of register 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 0, &reg);
	if ((reg & 0x0100) == 0) {
		reg |= 0x0100;
		status |=
		    interface->mppa_88E1111_write(interface->context, interface->chip_id, 0, reg);
		// Perform a soft reset
		status |= mppa_88E1111_copper_reset(interface);
	}
	return status;
}

int mppa_88E1111_copper_full_duplex_disable(mppa_88E1111_interface_t * interface)
{
	int status = 0;
	uint16_t reg;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Disabling copper full duplex mode.\n", interface->chip_id);
#endif
	status |= mppa_88E1111_copper_autoneg_disable(interface);
	// Select page 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = reg & 0xff00;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Clear bit 8 of register 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 0, &reg);
	if ((reg & 0x0100) != 0) {
		reg &= 0xfeff;
		status |=
		    interface->mppa_88E1111_write(interface->context, interface->chip_id, 0, reg);
		// Perform a soft reset
		status |= mppa_88E1111_copper_reset(interface);
	}
	return status;
}

int mppa_88E1111_fiber_full_duplex_enable(mppa_88E1111_interface_t * interface)
{
	int status = 0;
	uint16_t reg;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Activating fiber full duplex mode.\n", interface->chip_id);
#endif
	status |= mppa_88E1111_fiber_autoneg_disable(interface);
	// Select page 1
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = (reg & 0xff00) | 0x0001;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Set bit 8 of register 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 0, &reg);
	if ((reg & 0x0100) == 0) {
		reg |= 0x0100;
		status |=
		    interface->mppa_88E1111_write(interface->context, interface->chip_id, 0, reg);
		// Perform a soft reset
		status |= mppa_88E1111_fiber_reset(interface);
	}
	return status;
}

int mppa_88E1111_fiber_full_duplex_disable(mppa_88E1111_interface_t * interface)
{
	int status = 0;
	uint16_t reg;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Disabling fiber full duplex mode.\n", interface->chip_id);
#endif
	status |= mppa_88E1111_fiber_autoneg_disable(interface);
	// Select page 1
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = (reg & 0xff00) | 0x0001;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Clear bit 8 of register 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 0, &reg);
	if ((reg & 0x0100) != 0) {
		reg &= 0xfeff;
		status |=
		    interface->mppa_88E1111_write(interface->context, interface->chip_id, 0, reg);
		// Perform a soft reset
		status |= mppa_88E1111_fiber_reset(interface);
	}
	return status;
}

int mppa_88E1111_copper_loopback_disable(mppa_88E1111_interface_t * interface)
{
	int status = 0;
	uint16_t reg;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Disabling copper loopback.\n", interface->chip_id);
#endif
	// Select page 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = reg & 0xff00;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Clear bit 14 of register 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 0, &reg);
	reg &= 0xbfff;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 0, reg);
	return status;
}

int mppa_88E1111_fiber_loopback_disable(mppa_88E1111_interface_t * interface)
{
	int status = 0;
	uint16_t reg;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Disabling fiber loopback.\n", interface->chip_id);
#endif
	// Select page 1
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = (reg & 0xff00) | 0x0001;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Clear bit 14 of register 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 0, &reg);
	reg &= 0xbfff;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 0, reg);
	return status;
}

int mppa_88E1111_copper_get_loopback_status(mppa_88E1111_interface_t *
					    interface, uint8_t * loopback_enabled)
{
	int status = 0;
	uint16_t reg;

	// Select page 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = reg & 0xff00;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Get bit 14 of register 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 0, &reg);
	*loopback_enabled = (uint8_t) ((reg >> 14) & 0x01);
#ifdef VERBOSE
	if (*loopback_enabled == 1) {
		printf("[88E1111 0x%.2x] Copper loopback is enabled.\n", interface->chip_id);
	} else {
		printf("[88E1111 0x%.2x] Copper loopback is disabled.\n", interface->chip_id);
	}
#endif
	return status;
}

int mppa_88E1111_fiber_get_loopback_status(mppa_88E1111_interface_t *
					   interface, uint8_t * loopback_enabled)
{
	int status = 0;
	uint16_t reg;

	// Select page 1
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = (reg & 0xff00) | 0x0001;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Get bit 14 of register 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 0, &reg);
	*loopback_enabled = (uint8_t) ((reg >> 14) & 0x01);
#ifdef VERBOSE
	if (*loopback_enabled == 1) {
		printf("[88E1111 0x%.2x] Fiber loopback is enabled.\n", interface->chip_id);
	} else {
		printf("[88E1111 0x%.2x] Fiber loopback is disabled.\n", interface->chip_id);
	}
#endif
	return status;
}

int mppa_88E1111_copper_loopback_enable(mppa_88E1111_interface_t * interface)
{
	int status = 0;
	uint16_t reg;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Enabling copper loopback.\n", interface->chip_id);
#endif
	// Select page 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = reg & 0xff00;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Write bit 14 of register 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 0, &reg);
	reg |= 0x4000;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 0, reg);
	return status;
}

int mppa_88E1111_fiber_loopback_enable(mppa_88E1111_interface_t * interface)
{
	int status = 0;
	uint16_t reg;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Enabling fiber loopback.\n", interface->chip_id);
#endif
	// Select page 1
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = (reg & 0xff00) | 0x0001;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Write bit 14 of register 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 0, &reg);
	reg |= 0x4000;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 0, reg);
	return status;
}

int mppa_88E1111_line_get_loopback_status(mppa_88E1111_interface_t *
					  interface, uint8_t * loopback_enabled)
{
	int status = 0;
	uint16_t reg;

	// Get bit 14 of register 20
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 20, &reg);
	*loopback_enabled = (uint8_t) ((reg >> 14) & 0x01);
#ifdef VERBOSE
	if (*loopback_enabled == 1) {
		printf("[88E1111 0x%.2x] Line loopback is enabled.\n", interface->chip_id);
	} else {
		printf("[88E1111 0x%.2x] Line loopback is disabled.\n", interface->chip_id);
	}
#endif
	return status;
}

int mppa_88E1111_line_loopback_disable(mppa_88E1111_interface_t * interface)
{
	int status = 0;
	uint16_t reg;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Disabling line loopback.\n", interface->chip_id);
#endif
	// Clear bit 14 of register 20
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 20, &reg);
	reg &= 0xbfff;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 20, reg);
	return status;
}

int mppa_88E1111_line_loopback_enable(mppa_88E1111_interface_t * interface)
{
	int status = 0;
	uint16_t reg;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Enabling line loopback.\n", interface->chip_id);
#endif
	// Write bit 14 of register 20
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 20, &reg);
	reg |= 0x4000;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 20, reg);
	return status;
}

int mppa_88E1111_fiber_reset(mppa_88E1111_interface_t * interface)
{
	// Select page 1, write 1 in bit 15 of register 0.
	int status = 0;
	uint16_t reg;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Reseting fiber.\n", interface->chip_id);
#endif
	// Select page 1
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = (reg & 0xff00) | 0x0001;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Set reset bit of ctrl register
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 0, &reg);
	reg |= 0x8000;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 0, reg);
	// Wait for reset be done : reset bit of ctrl register will go back to low
	while ((reg & 0x8000) != 0) {
		status |=
		    interface->mppa_88E1111_read(interface->context, interface->chip_id, 0, &reg);
	}
	return status;
}

int mppa_88E1111_copper_reset(mppa_88E1111_interface_t * interface)
{
	// Select page 0, write 1 in bit 15 of register 0.
	int status = 0;
	uint16_t reg;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Reseting copper.\n", interface->chip_id);
#endif
	// Select page 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = reg & 0xff00;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Set reset bit of ctrl register
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 0, &reg);
	reg |= 0x8000;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 0, reg);
	// Wait for reset be done : reset bit of ctrl register will go back to low
	while ((reg & 0x8000) != 0) {
		status |=
		    interface->mppa_88E1111_read(interface->context, interface->chip_id, 0, &reg);
	}
	return status;
}

int mppa_88E1111_check_phy_identifier(mppa_88E1111_interface_t * interface)
{
	uint32_t phy_identifier;
	int status = 0;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Checking phy identifier.\n", interface->chip_id);
#endif
	status |= mppa_88E1111_get_phy_identifier(interface, &phy_identifier);
#ifdef VERBOSE
	printf("[88E1111 0x%.2x] => Identifier      : 0x%.6x\n",
	       interface->chip_id, (unsigned int) (phy_identifier >> 10));
	printf("[88E1111 0x%.2x] => Model    number : 0x%.2x\n",
	       interface->chip_id, (unsigned int) ((phy_identifier >> 4) & 0x0000003f));
	printf("[88E1111 0x%.2x] => Revision number : %u\n",
	       interface->chip_id, (unsigned int) (phy_identifier & 0x0000000f));
#endif
	if (phy_identifier != 0x01410cc2 || status != 0) {
		status = -1;
#ifdef VERBOSE
		printf
		    ("[88E1111 0x%.2x] => Identifier is not the expected one.\n",
		     interface->chip_id);
		printf("[88E1111 0x%.2x] => Failed.\n", interface->chip_id);
	} else {
		printf("[88E1111 0x%.2x] => Identifier is correct.\n", interface->chip_id);
		printf("[88E1111 0x%.2x] => Passed.\n", interface->chip_id);
#endif
	}
	return status;
}

int mppa_88E1111_get_phy_identifier(mppa_88E1111_interface_t * interface, uint32_t * phy_identifier)
{
	uint16_t reg;
	int status = 0;

	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 2, &reg);
	*phy_identifier = ((uint32_t) reg) << 16;
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 3, &reg);
	*phy_identifier |= (uint32_t) reg;
	return status;
}

#ifndef NO_INTERFACE_LIST
int mppa_88E1111_open(__mppa_platform_type_t platform,
		      mppa_88E1111_interface_list_t * interface_list)
{
	int i;
	volatile mppa_i2c_master_t *i2c_master;

	switch (platform) {
	case __MPPA_ETH_FPGA:
#ifdef VERBOSE
		printf
		    ("[88E1111] Opening a 88E1111 chip MDIO interface for MPPA Ethernet FPGA board.\n");
#endif
		interface_list->interface_nb = 1;
		for (i = 0; i < 4; i++) {
			interface_list->interface[i].i2c_master = 0;
		}
		interface_list->interface[0].mdio_master.mppa_clk_period_ps = 50000;
		interface_list->interface[0].chip_id = 0;
		interface_list->interface[0].context =
		    (void *) (&(interface_list->interface[0].mdio_master));
		interface_list->interface[0].mppa_88E1111_read = (int (*)
								  (volatile void *, uint8_t,
								   uint8_t,
								   uint16_t *)) mppa_eth_mdio_read;
		interface_list->interface[0].mppa_88E1111_write =
		    (int (*)(volatile void *, uint8_t, uint8_t, uint16_t))
		    mppa_eth_mdio_write;
		return mppa_eth_mdio_init((int)
					  (&(interface_list->interface[0].mdio_master)));
		break;
	case __MPPA_AB01:
#ifdef VERBOSE
		printf
		    ("[88E1111] Opening two 88E1111 chip I2C interface for MPPA Ethernet AB01 board.\n");
#endif
		/* Setup the two 1GB Ethernet interface */
		interface_list->interface_nb = 2;

		i2c_master = setup_i2c_master(0, 1, I2C_BITRATE, 0x0LL);

		interface_list->interface[0].i2c_master = i2c_master;
		interface_list->interface[0].i2c_bus = 2;
		interface_list->interface[0].i2c_coma_pin = 12;
		interface_list->interface[0].i2c_reset_n_pin = 13;
		interface_list->interface[0].i2c_int_n_pin = 14;
		interface_list->interface[0].i2c_gic = 0;
		interface_list->interface[0].chip_id = 0x40;

		interface_list->interface[1].i2c_master = i2c_master;
		interface_list->interface[1].i2c_bus = 2;
		interface_list->interface[1].i2c_coma_pin = 15;
		interface_list->interface[1].i2c_reset_n_pin = 16;
		interface_list->interface[1].i2c_int_n_pin = 14;
		interface_list->interface[1].i2c_gic = 0;
		interface_list->interface[1].chip_id = 0x41;

		for (i = 0; i < 2; i++) {
			interface_list->interface[i].context =
			    (void *) (interface_list->interface[i].i2c_master);
			interface_list->interface[i].mppa_88E1111_read = (int (*)
									  (volatile void *, uint8_t,
									   uint8_t,
									   uint16_t *))
			    mppa_i2c_register_read;
			interface_list->interface[i].mppa_88E1111_write = (int (*)
									   (volatile void *,
									    uint8_t, uint8_t,
									    uint16_t))
			    mppa_i2c_register_write;
			if (mppa_i2c_init
			    (interface_list->interface[i].i2c_master,
			     interface_list->interface[i].i2c_bus,
			     interface_list->interface[i].i2c_coma_pin,
			     interface_list->interface[i].i2c_reset_n_pin,
			     interface_list->interface[i].i2c_int_n_pin,
			     interface_list->interface[i].i2c_gic) != 0)
				return -1;
		}
		break;

	case __MPPA_SIM:
	case __MPPA_EMB01:
	default:
		return -1;
		break;
	}

	return 0;
}

int mppa_88E1111_close(mppa_88E1111_interface_list_t * interface_list)
{
	unsigned int i;
	int status = 0;

	for (i = 0; i < interface_list->interface_nb; i++) {
#ifdef VERBOSE
		printf("[88E1111 0x%.2x] Closing connection.\n",
		       interface_list->interface[i].chip_id);
#endif
		if (interface_list->interface[i].i2c_master == 0) {
			if (mppa_eth_mdio_finish((int)
						 &(interface_list->interface[i].mdio_master)) != 0)
				status = -1;
		}
	}
	return status;
}
#endif

int mppa_88E1111_dump_register(mppa_88E1111_interface_t * interface)
{
	int i;
	unsigned int j;
	int status = 0;
	uint16_t val;
	uint32_t page_reg[] = {
		0x7FFF87FF,	/* PAGE 0 */
		0x7FFF87FF,	/* PAGE 1 */
		0x10000000,	/* PAGE 2 */
		0x50000000,	/* PAGE 3 */
		0x10000000,	/* PAGE 4 */
		0x10000000,	/* PAGE 5 */
		0x00000000,	/* PAGE 6 */
		0x40000000,	/* PAGE 7 */
		0x00000000,	/* PAGE 8 */
		0x00000000,	/* PAGE 9 */
		0x00000000,	/* PAGE 10 */
		0x40000000,	/* PAGE 11 */
		0x40000000,	/* PAGE 12 */
		0x00000000,	/* PAGE 13 */
		0x00000000,	/* PAGE 14 */
		0x00000000,	/* PAGE 15 */
		0x40000000,	/* PAGE 16 */
		0x00000000,	/* PAGE 17 */
		0x40000000	/* PAGE 18 */
	};

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Dumping 88E1111 chip registers.\n", interface->chip_id);
#endif
	for (j = 0; j < (sizeof(page_reg) / sizeof(uint32_t)); j++) {
#ifdef VERBOSE
		printf("[88E1111 0x%.2x] ****** PAGE %d *******\n", interface->chip_id, j);
#endif
		for (i = 0; i < 32; i++) {
			if ((page_reg[j]) & (1 << i)) {
				/* Select Pages */
				if (i < 30) {
					status |=
					    interface->mppa_88E1111_write(interface->context,
									  interface->chip_id, 22,
									  j & 0x7F);
				} else {
					status |=
					    interface->mppa_88E1111_write(interface->context,
									  interface->chip_id, 29,
									  j & 0x7F);
				}
				if (status != 0) {
#ifdef VERBOSE
					printf
					    ("[88E1111 0x%.2x] 88E1111 register dump failed.\n",
					     interface->chip_id);
#endif
					return -1;
				}
				status |=
				    interface->mppa_88E1111_read(interface->context,
								 interface->chip_id, i, &val);
				if (status != 0) {
#ifdef VERBOSE
					printf
					    ("[88E1111 0x%.2x] 88E1111 register dump failed.\n",
					     interface->chip_id);
#endif
					return -1;
				}
#ifdef VERBOSE
				printf
				    ("[88E1111 0x%.2x] Register %d: 0x%.4x\n",
				     interface->chip_id, i, val);
#endif
			}
		}
	}
	/* return to pages 0 */
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, 0);
	return status;
}

int mppa_88E1111_copper_autoneg_enable(mppa_88E1111_interface_t * interface)
{
	uint16_t reg;
	int status = 0;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Enabling copper autoneg.\n", interface->chip_id);
#endif
	// Select page 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = reg & 0xff00;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Set bit 12 & 15 of register 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 0, &reg);
	reg |= 0x9000;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 0, reg);
	// Perform a soft reset
	status |= mppa_88E1111_copper_reset(interface);


	return status;
}

int mppa_88E1111_fiber_autoneg_enable(mppa_88E1111_interface_t * interface)
{
	uint16_t reg;
	int status = 0;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Enabling fiber autoneg.\n", interface->chip_id);
#endif
	// Select page 1
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = (reg & 0xff00) | 0x0001;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Set bit 12 & 15 of register 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 0, &reg);
	reg |= 0x9000;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 0, reg);
	return status;
}

int mppa_88E1111_copper_autoneg_disable(mppa_88E1111_interface_t * interface)
{
	uint16_t reg;
	int status = 0;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Disabling copper autoneg.\n", interface->chip_id);
#endif
	// Select page 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = reg & 0xff00;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Clear bit 12 and set bit 15 of register 0 
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 27, &reg);
	reg = (reg & 0xefff) | 0x8000;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 27, reg);
	// Perform a soft reset
//  status |= mppa_88E1111_copper_reset(interface);

	return status;
}

int mppa_88E1111_fiber_autoneg_enabled(mppa_88E1111_interface_t * interface, uint8_t * autoneg_en)
{
	// Get bit 12 of register 0 of page 1
	uint16_t reg;
	int status = 0;

	// Select page 1
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = (reg & 0xff00) | 0x0001;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Get bit 12 of register 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 0, &reg);
	*autoneg_en = (uint8_t) ((reg >> 12) & 0x0001);

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Fiber autoneg is ", interface->chip_id);
	if (*autoneg_en == 1) {
		printf("enabled.\n");
	} else {
		printf("disabled.\n");
	}
#endif
	return status;
}

int mppa_88E1111_fiber_autoneg_disable(mppa_88E1111_interface_t * interface)
{
	uint16_t reg;
	int status = 0;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Disabling fiber autoneg.\n", interface->chip_id);
#endif
	// Select page 1
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = (reg & 0xff00) | 0x0001;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Clear bit 12 and set bit 15 of register 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 0, &reg);
	reg = (reg & 0xefff) | 0x8000;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 0, reg);
	return status;
}

int mppa_88E1111_led_link10_on(mppa_88E1111_interface_t * interface)
{
	uint16_t reg;
	int status = 0;
#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Set LED link10 configuration to on.\n", interface->chip_id);
#endif

	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 25, &reg);
	reg = (reg & 0xfcff) | 0x0300;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 25, reg);
	return status;
}

int mppa_88E1111_led_link100_on(mppa_88E1111_interface_t * interface)
{
	uint16_t reg;
	int status = 0;
#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Set LED link100 configuration to on.\n", interface->chip_id);
#endif

	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 25, &reg);
	reg = (reg & 0xff3f) | 0x00c0;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 25, reg);
	return status;
}

int mppa_88E1111_led_link1000_on(mppa_88E1111_interface_t * interface)
{
	uint16_t reg;
	int status = 0;
#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Set LED link1000 configuration to on.\n", interface->chip_id);
#endif

	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 25, &reg);
	reg = (reg & 0xffcf) | 0x0030;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 25, reg);
	return status;
}

int mppa_88E1111_led_duplex_on(mppa_88E1111_interface_t * interface)
{
	uint16_t reg;
	int status = 0;
#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Set LED duplex configuration to on.\n", interface->chip_id);
#endif

	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 25, &reg);
	reg = (reg & 0xf3ff) | 0x0c00;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 25, reg);
	return status;
}

int mppa_88E1111_led_rx_on(mppa_88E1111_interface_t * interface)
{
	uint16_t reg;
	int status = 0;
#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Set LED Rx configuration to on.\n", interface->chip_id);
#endif

	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 25, &reg);
	reg = (reg & 0xfff3) | 0x000c;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 25, reg);
	return status;
}

int mppa_88E1111_led_tx_on(mppa_88E1111_interface_t * interface)
{
	uint16_t reg;
	int status = 0;
#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Set LED Tx configuration to on.\n", interface->chip_id);
#endif

	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 25, &reg);
	reg = (reg & 0xfffc) | 0x0003;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 25, reg);
	return status;
}

int mppa_88E1111_led_on(mppa_88E1111_interface_t * interface)
{
	uint16_t reg;
	int status = 0;
#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Set LED configuration to on.\n", interface->chip_id);
#endif

	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 25, &reg);
	reg = (reg & 0xf000) | 0x0fff;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 25, reg);
	return status;
}

int mppa_88E1111_led_off(mppa_88E1111_interface_t * interface)
{
	uint16_t reg;
	int status = 0;
#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Set LED configuration to off.\n", interface->chip_id);
#endif

	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 25, &reg);
	reg = (reg & 0xf000) | 0x0aaa;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 25, reg);
	return status;
}

int mppa_88E1111_led_blink(mppa_88E1111_interface_t * interface)
{
	uint16_t reg;
	int status = 0;
#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Set LED configuration to blink.\n", interface->chip_id);
#endif

	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 25, &reg);
	reg = (reg & 0xf000) | 0x0555;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 25, reg);
	return status;
}

int mppa_88E1111_get_led_mode(mppa_88E1111_interface_t * interface, uint16_t * mode)
{
	uint16_t reg;
	int status = 0;

	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 25, &reg);
	*mode = reg & 0x0fff;
#ifdef VERBOSE
	printf("[88E1111 0x%.2x] LED_DUPLEX   mode : ", interface->chip_id);
	reg = (*mode >> 10) & 0x0003;
	if (reg == 0)
		printf("auto.\n");
	if (reg == 1)
		printf("blink.\n");
	if (reg == 2)
		printf("on.\n");
	if (reg == 3)
		printf("off.\n");
	printf("[88E1111 0x%.2x] LED_LINK10   mode : ", interface->chip_id);
	reg = (*mode >> 8) & 0x0003;
	if (reg == 0)
		printf("auto.\n");
	if (reg == 1)
		printf("blink.\n");
	if (reg == 2)
		printf("on.\n");
	if (reg == 3)
		printf("off.\n");
	printf("[88E1111 0x%.2x] LED_LINK100  mode : ", interface->chip_id);
	reg = (*mode >> 6) & 0x0003;
	if (reg == 0)
		printf("auto.\n");
	if (reg == 1)
		printf("blink.\n");
	if (reg == 2)
		printf("on.\n");
	if (reg == 3)
		printf("off.\n");
	printf("[88E1111 0x%.2x] LED_LINK1000 mode : ", interface->chip_id);
	reg = (*mode >> 4) & 0x0003;
	if (reg == 0)
		printf("auto.\n");
	if (reg == 1)
		printf("blink.\n");
	if (reg == 2)
		printf("on.\n");
	if (reg == 3)
		printf("off.\n");
	printf("[88E1111 0x%.2x] LED_RX       mode : ", interface->chip_id);
	reg = (*mode >> 2) & 0x0003;
	if (reg == 0)
		printf("auto.\n");
	if (reg == 1)
		printf("blink.\n");
	if (reg == 2)
		printf("on.\n");
	if (reg == 3)
		printf("off.\n");
	printf("[88E1111 0x%.2x] LED_TX       mode : ", interface->chip_id);
	reg = *mode & 0x0003;
	if (reg == 0)
		printf("auto.\n");
	if (reg == 1)
		printf("blink.\n");
	if (reg == 2)
		printf("on.\n");
	if (reg == 3)
		printf("off.\n");
#endif
	return status;
}

int mppa_88E1111_led_auto(mppa_88E1111_interface_t * interface)
{
	uint16_t reg;
	int status = 0;
#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Set LED configuration to auto.\n", interface->chip_id);
#endif

	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 25, &reg);
	reg = reg & 0xf000;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 25, reg);
	return status;
}

int mppa_88E1111_copper_set_rate(mppa_88E1111_interface_t * interface, uint8_t rate)
{
	// Disable autoneg + set bit 13 & 6 of register 0 page 0 + soft reset
	uint16_t reg;
	int status = 0;

#ifdef VERBOSE
	switch (rate) {
	case 0:
		printf("[88E1111 0x%.2x] Setting copper rate to 10Mb/s.\n", interface->chip_id);
		break;
	case 1:
		printf("[88E1111 0x%.2x] Setting copper rate to 100Mb/s.\n", interface->chip_id);
		break;
	case 2:
		printf("[88E1111 0x%.2x] Setting copper rate to 1Gb/s.\n", interface->chip_id);
		break;
	default:
		break;
	}
#endif
	if (rate > 2)
		return -1;
	status |= mppa_88E1111_copper_autoneg_disable(interface);
	// Select page 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = reg & 0xff00;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Set bit 13 & 6 of register 0 page 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 0, &reg);
	reg &= 0xdfbf;
	reg |= (rate << 5) & 0x0040;
	reg |= (rate << 13) & 0x2000;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 0, reg);
	// Perform a soft reset
	status |= mppa_88E1111_copper_reset(interface);
	return status;
}

int mppa_88E1111_fiber_get_rate(mppa_88E1111_interface_t * interface, uint8_t * rate)
{
	// First check fiber auto-neg status.
	// If auto-neg is running, return bit 13 & 6 of register 0 page 1
	// Else return default speed

	uint16_t reg;
	int status = 0;

	// Get fiber auto-neg status
	mppa_88E1111_fiber_autoneg_enabled(interface, (uint8_t *) & reg);
	if (reg == 0) {
		status = mppa_88E1111_get_default_speed(interface, rate);
	} else {
		// Select page 1
		status |=
		    interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
		reg = (reg & 0xff00) | 0x0001;
		status |=
		    interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
		// Read register status
		status |=
		    interface->mppa_88E1111_read(interface->context, interface->chip_id, 0, &reg);
		*rate = ((reg >> 5) & 0x2) | ((reg >> 13) & 0x1);
		if (*rate > 2)
			status = -1;
	}
#ifdef VERBOSE
	switch (*rate) {
	case 0:
		printf("[88E1111 0x%.2x] Fiber rate is set to 10Mb/s.\n", interface->chip_id);
		break;
	case 1:
		printf("[88E1111 0x%.2x] Fiber rate is set to 100Mb/s.\n", interface->chip_id);
		break;
	case 2:
		printf("[88E1111 0x%.2x] Fiber rate is set to 1Gb/s.\n", interface->chip_id);
		break;
	default:
		printf
		    ("[88E1111 0x%.2x] Fiber rate is set to an unexpected value 0x%.2x.\n",
		     interface->chip_id, *rate);
		break;
	}
#endif
	return status;
}

int mppa_88E1111_copper_get_rate(mppa_88E1111_interface_t * interface, uint8_t * rate)
{
	// Get bit 13 & 6 of register 0 page 0
	uint16_t reg;
	int status = 0;

	// Select page 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = reg & 0xff00;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Read register status
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 0, &reg);
	*rate = ((reg >> 5) & 0x2) | ((reg >> 13) & 0x1);
	if (*rate > 2)
		status = -1;
#ifdef VERBOSE
	switch (*rate) {
	case 0:
		printf("[88E1111 0x%.2x] Copper rate is set to 10Mb/s.\n", interface->chip_id);
		break;
	case 1:
		printf("[88E1111 0x%.2x] Copper rate is set to 100Mb/s.\n", interface->chip_id);
		break;
	case 2:
		printf("[88E1111 0x%.2x] Copper rate is set to 1Gb/s.\n", interface->chip_id);
		break;
	default:
		printf
		    ("[88E1111 0x%.2x] Copper rate is set to an unexpected value 0x%.2x.\n",
		     interface->chip_id, *rate);
		break;
	}
#endif
	return status;
}

int mppa_88E1111_configure(mppa_88E1111_interface_t * interface)
{
	if (mppa_88E1111_check_phy_identifier(interface) != 0)
		return -1;
	if (mppa_88E1111_set_mdi_crossover_auto_mode(interface) != 0)
		return -1;
	if (mppa_88E1111_led_auto(interface) != 0)
		return -1;
	if (mppa_88E1111_fiber_autoneg_disable(interface) != 0)
		return -1;
	if (mppa_88E1111_copper_autoneg_enable(interface) != 0)
		return -1;
	return 0;
}

int mppa_88E1111_configure_all(mppa_88E1111_interface_list_t * interface_list)
{
	unsigned int i;

	for (i = 0; i < interface_list->interface_nb; i++) {
		if (mppa_88E1111_configure(&(interface_list->interface[i]))
		    != 0)
			return -1;
	}
	return 0;
}

int mppa_88E1111_synchronize(mppa_88E1111_interface_t * interface)
{
	uint16_t reg;
	int status = 0;

	//Select page 1
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = (reg & 0xff00) | 0x0001;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	if (status != 0)
		return status;

	unsigned long long start = __k1_read_dsu_timestamp();
	int up = 0;
	while (__k1_read_dsu_timestamp() - start < 5ULL * __bsp_frequency) {
		// Get bit 15 of register 4
		status |=
		    interface->mppa_88E1111_read(interface->context, interface->chip_id, 4, &reg);
		if (status != 0)
			return status;
		if ((reg >> 15) == 1) {
			up = 1;
			break;
		}
		__k1_cpu_backoff(1000);
	}
#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Link %s.\n", interface->chip_id, up ? "up" : "down/polling");
#endif
	return !up;
}

int mppa_88E1111_synchronize_all(mppa_88E1111_interface_list_t * interface_list)
{
	unsigned int i;

	for (i = 0; i < interface_list->interface_nb; i++) {
		if (mppa_88E1111_synchronize(&(interface_list->interface[i])) != 0)
			return -1;
	}
	return 0;
}

int mppa_88E1111_set_default_speed(mppa_88E1111_interface_t * interface, uint8_t speed)
{
	// Set bit 6:4 of register 20. Valid only if auto-neg is enabled.
	uint16_t reg;
	int status = 0;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Setting default speed to ", interface->chip_id);
	switch (speed & 0x7) {
	case 0:
	case 4:
		printf("10Mb/s.\n");
		break;
	case 1:
	case 5:
		printf("100Mb/s.\n");
		break;
	default:
		printf("1Gb/s.\n");
		break;
	}
#endif

	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 20, &reg);
	reg = (reg & 0xff8f) | ((speed << 4) & 0x0070);
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 20, reg);
	// Perform a soft reset
	status |= mppa_88E1111_fiber_reset(interface);
	status |= mppa_88E1111_copper_reset(interface);
	return status;
}

int mppa_88E1111_get_default_speed(mppa_88E1111_interface_t * interface, uint8_t * speed)
{
	// Get bit 6:4 of register 20
	uint16_t reg;
	int status = 0;

	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 20, &reg);
	*speed = (reg >> 4) & 0x0007;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Default speed is set to ", interface->chip_id);
	switch (*speed & 0x7) {
	case 0:
	case 4:
		printf("10Mb/s.\n");
		break;
	case 1:
	case 5:
		printf("100Mb/s.\n");
		break;
	default:
		printf("1Gb/s.\n");
		break;
	}
#endif
	return status;
}

int mppa_88E1111_disable_fiber_copper_auto_selection(mppa_88E1111_interface_t * interface)
{
	// Set bit 15 of register 27
	uint16_t reg;
	int status = 0;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Disabling fiber/copper auto selection.\n", interface->chip_id);
#endif
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 27, &reg);
	reg |= 0x8000;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 27, reg);
	// Perform a soft reset
	status |= mppa_88E1111_fiber_reset(interface);
	status |= mppa_88E1111_copper_reset(interface);
	return status;
}

int mppa_88E1111_enable_fiber_copper_auto_selection(mppa_88E1111_interface_t * interface)
{
	// Clear bit 15 of register 27
	uint16_t reg;
	int status = 0;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Enabling fiber/copper auto selection.\n", interface->chip_id);
#endif
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 27, &reg);
	reg &= 0x7fff;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 27, reg);
	// Perform a soft reset
	status |= mppa_88E1111_fiber_reset(interface);
	status |= mppa_88E1111_copper_reset(interface);
	return status;
}

int mppa_88E1111_get_fiber_copper_auto_selection(mppa_88E1111_interface_t * interface, uint8_t * en)
{
	// Get bit 15 of register 27
	uint16_t reg;
	int status = 0;

	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 27, &reg);
	*en = 1;
	if ((reg & 0x8000) == 0)
		*en = 0;
#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Fiber/copper auto selection is ", interface->chip_id);
	if ((reg & 0x8000) == 0) {
		printf("enabled.\n");
	} else {
		printf("disabled.\n");
	}
#endif

	return status;
}

int mppa_88E1111_enable_packet_generator(mppa_88E1111_interface_t * interface)
{
	// Configure register 30 of page 18
	uint16_t reg;
	int status = 0;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Enabling packet generator.\n", interface->chip_id);
#endif
	// Select page 18
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 29, &reg);
	reg = (reg & 0xffe0) | 0x0012;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 29, reg);
	// Configure register 30 of page 18
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 30, &reg);
	reg |= 0x0038;		// Generate pkt withour error of length of 1518 bytes
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 30, reg);

	return status;
}

int mppa_88E1111_disable_packet_generator(mppa_88E1111_interface_t * interface)
{
	// Configure register 30 of page 18
	uint16_t reg;
	int status = 0;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Disabling packet generator.\n", interface->chip_id);
#endif
	// Select page 18
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 29, &reg);
	reg = (reg & 0xffe0) | 0x0012;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 29, reg);
	// Configure register 30 of page 18
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 30, &reg);
	reg &= 0xffdf;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 30, reg);

	return status;
}

int mppa_88E1111_get_packet_generator_status(mppa_88E1111_interface_t * interface, uint8_t * stat)
{
	// Get register 30 of page 18
	uint16_t reg;
	int status = 0;

	// Select page 18
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 29, &reg);
	reg = (reg & 0xffe0) | 0x0012;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 29, reg);
	// Read register 30
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 30, &reg);

	*stat = (((uint8_t) reg) >> 2) & 0x0f;

#ifdef VERBOSE
	if ((*stat & 0x08) == 0) {
		printf("[88E1111 0x%.2x] Packet generator is disabled.\n", interface->chip_id);
	} else {
		printf("[88E1111 0x%.2x] Packet generator is enabled.\n", interface->chip_id);
	}
	if ((*stat & 0x04) == 0) {
		printf
		    ("[88E1111 0x%.2x] Packet generator is using random data.\n",
		     interface->chip_id);
	} else {
		printf
		    ("[88E1111 0x%.2x] Packet generator is using 0x5a data.\n", interface->chip_id);
	}
	if ((*stat & 0x02) == 0) {
		printf
		    ("[88E1111 0x%.2x] Packet generator length : 64 bytes.\n", interface->chip_id);
	} else {
		printf
		    ("[88E1111 0x%.2x] Packet generator length : 1518 bytes.\n",
		     interface->chip_id);
	}
	if ((*stat & 0x01) == 0) {
		printf
		    ("[88E1111 0x%.2x] Packet generator error insertion is disabled.\n",
		     interface->chip_id);
	} else {
		printf
		    ("[88E1111 0x%.2x] Packet generator error insertion is enabled.\n",
		     interface->chip_id);
	}
#endif
	return status;
}

int mppa_88E1111_get_mdi_crossover_mode(mppa_88E1111_interface_t * interface, uint8_t * mode)
{
	// Return bits 6:5 of register 16
	uint16_t reg;
	int status = 0;

	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 16, &reg);
	*mode = (uint8_t) ((reg >> 5) & 0x3);
#ifdef VERBOSE
	printf("[88E1111 0x%.2x] MDI crossover mode is set to ", interface->chip_id);
	switch (*mode) {
	case 0:
		printf("manual MDI.\n");
		break;
	case 1:
		printf("manual MDIX.\n");
		break;
	case 3:
		printf("automatic.\n");
		break;
	default:
		printf("unknown mode.\n");
		break;
	}
#endif
	return status;
}

int mppa_88E1111_set_mdi_crossover_mode(mppa_88E1111_interface_t * interface, uint8_t mode)
{
	// Set bits 6:5 of register 16
	uint16_t reg;
	int status = 0;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Configuring MDI crossover mode to ", interface->chip_id);
	switch (mode) {
	case 0:
		printf("manual MDI.\n");
		break;
	case 1:
		printf("manual MDIX.\n");
		break;
	case 3:
		printf("automatic.\n");
		break;
	default:
		printf("unknown mode.\n");
		break;
	}
#endif
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 16, &reg);
	reg = (reg & 0xff9f) | (((uint16_t) mode << 5) & 0x0060);
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 16, reg);
	return status;
}

int mppa_88E1111_set_mdi_crossover_auto_mode(mppa_88E1111_interface_t * interface)
{
	return mppa_88E1111_set_mdi_crossover_mode(interface, 3);
}

int mppa_88E1111_led_disable(mppa_88E1111_interface_t * interface)
{
	// Set bit 15 of register 24
	uint16_t reg;
	int status = 0;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Disabling LED.\n", interface->chip_id);
#endif
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 24, &reg);
	reg |= 0x8000;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 24, reg);

	return status;
}

int mppa_88E1111_led_enable(mppa_88E1111_interface_t * interface)
{
	// Clear bit 15 of register 24
	uint16_t reg;
	int status = 0;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Enabling LED.\n", interface->chip_id);
#endif
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 24, &reg);
	reg &= 0x7fff;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 24, reg);

	return status;
}

int mppa_88E1111_led_enabled(mppa_88E1111_interface_t * interface, uint8_t * led_en)
{
	// Get bit 15 of register 24
	uint16_t reg;
	int status = 0;

	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 24, &reg);
	*led_en = (uint8_t) ((reg >> 15) & 0x0001);
#ifdef VERBOSE
	if (*led_en == 0) {
		printf("[88E1111 0x%.2x] LED are enabled.\n", interface->chip_id);
	} else {
		printf("[88E1111 0x%.2x] LED are disabled.\n", interface->chip_id);
	}
#endif
	return status;
}

int mppa_88E1111_copper_autoneg_complete(mppa_88E1111_interface_t *
					 interface, uint8_t * autoneg_done)
{
	// Get bit 5 of register 1 of page 0
	uint16_t reg;
	int status = 0;

	// Select page 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = reg & 0xff00;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Get bit 5 of register 1
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 1, &reg);
	*autoneg_done = (uint8_t) ((reg >> 5) & 0x0001);

#ifdef VERBOSE
	if (*autoneg_done == 1) {
		printf("[88E1111 0x%.2x] Copper autoneg completed.\n", interface->chip_id);
	} else {
		printf("[88E1111 0x%.2x] Copper autoneg not completed.\n", interface->chip_id);
	}
#endif
	return status;
}

int mppa_88E1111_copper_get_autoneg_ability(mppa_88E1111_interface_t *
					    interface, uint8_t * autoneg_en)
{
	// Get bit 3 of register 1 of page 0
	uint16_t reg;
	int status = 0;

	// Select page 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = reg & 0xff00;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Get bit 5 of register 1
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 1, &reg);
	*autoneg_en = (uint8_t) ((reg >> 3) & 0x0001);

#ifdef VERBOSE
	if (*autoneg_en == 1) {
		printf("[88E1111 0x%.2x] Copper autoneg is possible.\n", interface->chip_id);
	} else {
		printf("[88E1111 0x%.2x] Copper autoneg is not possible.\n", interface->chip_id);
	}
#endif
	return status;
}

int mppa_88E1111_fiber_get_link_status(mppa_88E1111_interface_t * interface, uint8_t * link_en)
{
	// Get bit 10 of register 17 - page 1
	uint16_t reg;
	int status = 0;

	// Select page 1
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = (reg & 0xff00) | 0x0001;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Get bit 10 of register 17 - page 1
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 17, &reg);
	*link_en = (uint8_t) ((reg >> 10) & 0x0001);

#ifdef VERBOSE
	if (*link_en == 1) {
		printf("[88E1111 0x%.2x] Fiber link is up.\n", interface->chip_id);
	} else {
		printf("[88E1111 0x%.2x] Fiber link is down.\n", interface->chip_id);
	}
#endif
	return status;
}

int mppa_88E1111_copper_get_link_status(mppa_88E1111_interface_t * interface, uint8_t * link_en)
{
	// Get bit 15 of register 4 of page 1
	uint16_t reg;
	int status = 0;

	// Select page 1
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = (reg & 0xff00) | 0x0001;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Get bit 15 of register 4
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 4, &reg);
	*link_en = (uint8_t) ((reg >> 15) & 0x0001);

#ifdef VERBOSE
	if (*link_en == 1) {
		printf("[88E1111 0x%.2x] Copper link detected.\n", interface->chip_id);
	} else {
		printf("[88E1111 0x%.2x] Copper link not detected.\n", interface->chip_id);
	}
#endif
	return status;
}

int mppa_88E1111_copper_get_status(mppa_88E1111_interface_t * interface, uint16_t * st)
{
	// Get bit 15 of register 4 of page 1
	uint16_t reg;
	int status = 0;

	// Select page 1
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = (reg & 0xff00) | 0x0001;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Get bit 15 of register 4
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 17, &reg);
	*st = reg;
	return status;
}

int mppa_88E1111_copper_get_energy_detect_mode(mppa_88E1111_interface_t * interface, uint8_t * mode)
{
	// Get bit 9:8 of register 16
	uint16_t reg;
	int status = 0;

	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 16, &reg);
	*mode = (uint8_t) ((reg >> 8) & 0x0003);

#ifdef VERBOSE
	switch (*mode) {
	case 2:
		printf
		    ("[88E1111 0x%.2x] Energy detect is set to \"Sense only on receive\".\n",
		     interface->chip_id);
		break;
	case 3:
		printf
		    ("[88E1111 0x%.2x] Energy detect is set to \"Sense and perdiodically transmit NLP\".\n",
		     interface->chip_id);
		break;
	default:
		printf("[88E1111 0x%.2x] Energy detect is set to \"Off\".\n", interface->chip_id);
		break;
	}
#endif
	return status;
}

int mppa_88E1111_copper_set_energy_detect_mode(mppa_88E1111_interface_t * interface, uint8_t mode)
{
	// Set bit 9:8 of register 16
	uint16_t reg;
	int status = 0;

#ifdef VERBOSE
	switch (mode) {
	case 2:
		printf
		    ("[88E1111 0x%.2x] Setting energy detect is to \"Sense only on receive\".\n",
		     interface->chip_id);
		break;
	case 3:
		printf
		    ("[88E1111 0x%.2x] Setting energy detect to \"Sense and perdiodically transmit NLP\".\n",
		     interface->chip_id);
		break;
	default:
		printf("[88E1111 0x%.2x] Setting energy detect to \"Off\".\n", interface->chip_id);
		break;
	}
#endif
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 16, &reg);
	reg = (reg & 0xfcff) | ((((uint16_t) mode) << 8) & 0x0300);
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 16, reg);

	return status;
}

int mppa_88E1111_fiber_get_energy_detect_status(mppa_88E1111_interface_t *
						interface, uint8_t * energy_detected)
{
	// Get bit 4 of register 17 - page 1
	uint16_t reg;
	int status = 0;

	// Select page 1
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = (reg & 0xff00) | 0x0001;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Get bit 4 of register 17 - page 1
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 17, &reg);
	*energy_detected = (uint8_t) ((reg >> 4) & 0x0001);

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Fiber energy detect status : ", interface->chip_id);
	if (*energy_detected == 0) {
		printf("Energy detected.\n");
	} else {
		printf("No energy detected.\n");
	}
#endif
	return status;
}

int mppa_88E1111_copper_get_energy_detect_status(mppa_88E1111_interface_t *
						 interface, uint8_t * sleep)
{
	// Get bit 4 of register 17 - page 0
	uint16_t reg;
	int status = 0;

	// Select page 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
	reg = reg & 0xff00;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
	// Get bit 4 of register 17 - page 0
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 17, &reg);
	*sleep = (uint8_t) ((reg >> 4) & 0x0001);

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Copper energy detect status : ", interface->chip_id);
	if (*sleep == 0) {
		printf("Active.\n");
	} else {
		printf("Sleep.\n");
	}
#endif
	return status;
}

int mppa_88E1111_force_good_link(mppa_88E1111_interface_t * interface)
{
	// Set bit 10 of register 16
	uint16_t reg;
	int status = 0;
#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Forcing good link.\n", interface->chip_id);
#endif
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 16, &reg);
	reg |= 0x0400;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 16, reg);

	return status;
}

int mppa_88E1111_virtual_cable_tester(mppa_88E1111_interface_t * interface, uint8_t * lane_status)
{
	uint16_t reg;
	int status = 0;
	uint16_t lane_id = 0;
	uint8_t current_lane_status;

	*lane_status = 0;
	for (lane_id = 0; lane_id < 4; lane_id++) {
#ifdef VERBOSE
		printf
		    ("[88E1111 0x%.2x] Performing VCT test on lane[%u].\n",
		     interface->chip_id, lane_id);
#endif
		// Select page "i"
		status |=
		    interface->mppa_88E1111_read(interface->context, interface->chip_id, 22, &reg);
		reg = (reg & 0xff00) | lane_id;
		status |=
		    interface->mppa_88E1111_write(interface->context, interface->chip_id, 22, reg);
		// Enable VCT test
		status |=
		    interface->mppa_88E1111_read(interface->context, interface->chip_id, 28, &reg);
		reg |= 0x8000;
		status |=
		    interface->mppa_88E1111_write(interface->context, interface->chip_id, 28, reg);
		// Wait for some time....
		__k1_cpu_backoff(0x01ffffff);
		// Get status
		status |=
		    interface->mppa_88E1111_read(interface->context, interface->chip_id, 28, &reg);
		current_lane_status = (uint8_t) ((reg >> 13) & 0x03);
		*lane_status |= current_lane_status << (2 * lane_id);
#ifdef VERBOSE
		printf("[88E1111 0x%.2x] VCT lane[%u] status : ", interface->chip_id, lane_id);
		switch (current_lane_status) {
		case 0:
			printf("normal cable.\n");
			break;
		case 1:
			printf("short in cable.\n");
			break;
		case 2:
			printf("open in cable.\n");
			break;
		default:
			printf("failed.\n");
			break;
		}
#endif
		// Stop VCT test
		status |=
		    interface->mppa_88E1111_read(interface->context, interface->chip_id, 28, &reg);
		reg &= 0x7fff;
		status |=
		    interface->mppa_88E1111_write(interface->context, interface->chip_id, 28, reg);
	}

	return status;
}

int mppa_88E1111_stub_loopback_enable(mppa_88E1111_interface_t * interface)
{
	uint16_t reg;
	int status = 0;

#ifdef VERBOSE
	printf("[88E1111 0x%.2x] Enabling stub loopback.\n", interface->chip_id);
#endif

	// Disable all interrupts
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 18, 0);
	// Force master : set bit 12:11 of register 9
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 9, &reg);
	reg |= 0x1800;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 9, reg);
	// Select page 7 of register 30
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 29, 7);
	// Set bit 3 of register 30
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 30, &reg);
	reg |= 0x0008;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 30, reg);
	// Select page 16 of register 30
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 29, 16);
	// Set bit 1 of register 30
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 30, &reg);
	reg |= 0x0002;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 30, reg);
	// Select page 18 of register 30
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 29, 18);
	// Set bit 0 of register 30
	status |= interface->mppa_88E1111_read(interface->context, interface->chip_id, 30, &reg);
	reg |= 0x0001;
	status |= interface->mppa_88E1111_write(interface->context, interface->chip_id, 30, reg);

	return status;
}
