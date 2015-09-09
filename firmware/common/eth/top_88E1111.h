#ifndef __HAL_88E1111_H__
#define __HAL_88E1111_H__

#include "mppa_ethernet_private.h"
#include "mppa_ethernet_shared.h"
#include "mdio_88E1111.h"
#include "i2c_88E1111.h"

typedef struct mppa_88E1111_interface {
	volatile mppa_i2c_master_t *i2c_master;
	int i2c_bus;
	int i2c_coma_pin;
	int i2c_reset_n_pin;
	int i2c_int_n_pin;
	int i2c_gic;
	mppa_eth_mdio_delay_context_t mdio_master;
	uint8_t chip_id;
	volatile void *context;
	int (*mppa_88E1111_read) (volatile void *context, uint8_t chip_id,
				  uint8_t reg, uint16_t * val);
	int (*mppa_88E1111_write) (volatile void *context, uint8_t chip_id,
				   uint8_t reg, uint16_t val);
} mppa_88E1111_interface_t;

#define CHIP_88E1111_NB 4

typedef struct mppa_88E1111_interface_list {
	unsigned int interface_nb;
	mppa_88E1111_interface_t interface[CHIP_88E1111_NB];
} mppa_88E1111_interface_list_t;

int mppa_88E1111_open(__mppa_platform_type_t platform,
		      mppa_88E1111_interface_list_t * interface_list);
int mppa_88E1111_close(mppa_88E1111_interface_list_t * interface_list);
int mppa_88E1111_configure(mppa_88E1111_interface_t * interface);
int mppa_88E1111_configure_all(mppa_88E1111_interface_list_t * interface_list);
int mppa_88E1111_synchronize(mppa_88E1111_interface_t * interface);
int mppa_88E1111_synchronize_all(mppa_88E1111_interface_list_t * interface_list);
int mppa_88E1111_dump_register(mppa_88E1111_interface_t * interface);
int mppa_88E1111_fiber_autoneg_enabled(mppa_88E1111_interface_t * interface, uint8_t * autoneg_en);
int mppa_88E1111_fiber_autoneg_disable(mppa_88E1111_interface_t * interface);
int mppa_88E1111_fiber_autoneg_enable(mppa_88E1111_interface_t * interface);
int mppa_88E1111_copper_autoneg_disable(mppa_88E1111_interface_t * interface);
int mppa_88E1111_copper_autoneg_enable(mppa_88E1111_interface_t * interface);
int mppa_88E1111_led_disable(mppa_88E1111_interface_t * interface);
int mppa_88E1111_led_enable(mppa_88E1111_interface_t * interface);
int mppa_88E1111_led_enabled(mppa_88E1111_interface_t * interface, uint8_t * led_en);
int mppa_88E1111_led_on(mppa_88E1111_interface_t * interface);
int mppa_88E1111_led_link10_on(mppa_88E1111_interface_t * interface);
int mppa_88E1111_led_link100_on(mppa_88E1111_interface_t * interface);
int mppa_88E1111_led_link1000_on(mppa_88E1111_interface_t * interface);
int mppa_88E1111_led_tx_on(mppa_88E1111_interface_t * interface);
int mppa_88E1111_led_rx_on(mppa_88E1111_interface_t * interface);
int mppa_88E1111_led_duplex_on(mppa_88E1111_interface_t * interface);
int mppa_88E1111_led_off(mppa_88E1111_interface_t * interface);
int mppa_88E1111_led_blink(mppa_88E1111_interface_t * interface);
int mppa_88E1111_led_auto(mppa_88E1111_interface_t * interface);
int mppa_88E1111_get_led_mode(mppa_88E1111_interface_t * interface, uint16_t * mode);
int mppa_88E1111_get_phy_identifier(mppa_88E1111_interface_t * interface,
				    uint32_t * phy_identifier);
int mppa_88E1111_check_phy_identifier(mppa_88E1111_interface_t * interface);
int mppa_88E1111_copper_reset(mppa_88E1111_interface_t * interface);
int mppa_88E1111_fiber_reset(mppa_88E1111_interface_t * interface);
int mppa_88E1111_line_loopback_enable(mppa_88E1111_interface_t * interface);
int mppa_88E1111_line_loopback_disable(mppa_88E1111_interface_t * interface);
int mppa_88E1111_line_get_loopback_status(mppa_88E1111_interface_t *
					  interface, uint8_t * loopback_enabled);
int mppa_88E1111_copper_loopback_enable(mppa_88E1111_interface_t * interface);
int mppa_88E1111_copper_loopback_disable(mppa_88E1111_interface_t * interface);
int mppa_88E1111_copper_get_loopback_status(mppa_88E1111_interface_t *
					    interface, uint8_t * loopback_enabled);
int mppa_88E1111_fiber_loopback_enable(mppa_88E1111_interface_t * interface);
int mppa_88E1111_fiber_loopback_disable(mppa_88E1111_interface_t * interface);
int mppa_88E1111_fiber_get_loopback_status(mppa_88E1111_interface_t *
					   interface, uint8_t * loopback_enabled);
int mppa_88E1111_copper_full_duplex_enable(mppa_88E1111_interface_t * interface);
int mppa_88E1111_copper_full_duplex_disable(mppa_88E1111_interface_t * interface);
int mppa_88E1111_fiber_full_duplex_enable(mppa_88E1111_interface_t * interface);
int mppa_88E1111_fiber_full_duplex_disable(mppa_88E1111_interface_t * interface);
int mppa_88E1111_fiber_get_duplex_mode(mppa_88E1111_interface_t * interface, uint8_t * duplex);
int mppa_88E1111_copper_get_duplex_mode(mppa_88E1111_interface_t * interface, uint8_t * duplex);
int mppa_88E1111_copper_get_real_rate(mppa_88E1111_interface_t * interface, uint8_t * rate);
int mppa_88E1111_copper_get_real_duplex_mode(mppa_88E1111_interface_t * interface, uint8_t * rate);
int mppa_88E1111_fiber_get_rate(mppa_88E1111_interface_t * interface, uint8_t * rate);
int mppa_88E1111_copper_get_rate(mppa_88E1111_interface_t * interface, uint8_t * rate);
int mppa_88E1111_copper_set_rate(mppa_88E1111_interface_t * interface, uint8_t rate);
int mppa_88E1111_get_mode(mppa_88E1111_interface_t * interface, uint8_t * mode);
int mppa_88E1111_set_mode(mppa_88E1111_interface_t * interface, uint8_t mode);
int mppa_88E1111_set_default_speed(mppa_88E1111_interface_t * interface, uint8_t speed);
int mppa_88E1111_get_default_speed(mppa_88E1111_interface_t * interface, uint8_t * speed);
int mppa_88E1111_enable_fiber_copper_auto_selection(mppa_88E1111_interface_t * interface);
int mppa_88E1111_disable_fiber_copper_auto_selection(mppa_88E1111_interface_t * interface);
int mppa_88E1111_get_fiber_copper_auto_selection(mppa_88E1111_interface_t *
						 interface, uint8_t * en);
int mppa_88E1111_enable_packet_generator(mppa_88E1111_interface_t * interface);
int mppa_88E1111_disable_packet_generator(mppa_88E1111_interface_t * interface);
int mppa_88E1111_get_packet_generator_status(mppa_88E1111_interface_t * interface, uint8_t * stat);
int mppa_88E1111_get_mdi_crossover_mode(mppa_88E1111_interface_t * interface, uint8_t * mode);
int mppa_88E1111_set_mdi_crossover_mode(mppa_88E1111_interface_t * interface, uint8_t mode);
int mppa_88E1111_set_mdi_crossover_auto_mode(mppa_88E1111_interface_t * interface);
int mppa_88E1111_copper_autoneg_complete(mppa_88E1111_interface_t *
					 interface, uint8_t * autoneg_done);
int mppa_88E1111_copper_get_autoneg_ability(mppa_88E1111_interface_t *
					    interface, uint8_t * autoneg_en);
int mppa_88E1111_copper_get_link_status(mppa_88E1111_interface_t * interface, uint8_t * link_en);
int mppa_88E1111_fiber_get_link_status(mppa_88E1111_interface_t * interface, uint8_t * link_en);
int mppa_88E1111_force_good_link(mppa_88E1111_interface_t * interface);
int mppa_88E1111_copper_get_energy_detect_mode(mppa_88E1111_interface_t *
					       interface, uint8_t * mode);
int mppa_88E1111_copper_set_energy_detect_mode(mppa_88E1111_interface_t * interface, uint8_t mode);
int mppa_88E1111_copper_get_energy_detect_status(mppa_88E1111_interface_t *
						 interface, uint8_t * sleep);
int mppa_88E1111_fiber_get_energy_detect_status(mppa_88E1111_interface_t *
						interface, uint8_t * energy_detected);
int mppa_88E1111_virtual_cable_tester(mppa_88E1111_interface_t * interface, uint8_t * lane_status);
int mppa_88E1111_stub_loopback_enable(mppa_88E1111_interface_t * interface);
int mppa_88E1111_copper_get_status(mppa_88E1111_interface_t * interface, uint16_t * st);

#endif
