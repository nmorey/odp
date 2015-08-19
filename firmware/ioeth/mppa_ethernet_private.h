#ifndef _mppa_ethernet_private_h_
#define _mppa_ethernet_private_h_

typedef enum {
	__MPPA_ETH_FPGA,
	__MPPA_AB01,
	__MPPA_SIM,
	__MPPA_EMB01
} __mppa_platform_type_t;


/* needs to be a multiple of 16 to have the same alignement for all packets */
#define MTU 1600

#define _ETH_DNOC_INTERFACE_ID 4	// Interface id on IOETH
#define UNUSED(x) (void)(x)

unsigned int mppa_eth_mdio_startup_ifce(unsigned int lane);

#endif /*_mppa_ethernet_private_h_*/
