#ifndef MDIO_UTILS_H
#define MDIO_UTILS_H
#include "bsp_phy.h"
#include "top_88E1111.h"
int dump_registers()
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
	for (j = 0; j < (sizeof(page_reg) / sizeof(uint32_t)); j++) {
		printf("[88E1111 0x] ****** PAGE %d *******\n", j);
		for (i = 0; i < 32; i++) {
			if ((page_reg[j]) & (1 << i)) {
				/* Select Pages */
				if (i < 30) {
					status |= mppa_eth_mdio_write(PHY, CHIP_ID, 22, j & 0x7F);
				} else {
					status |= mppa_eth_mdio_write(PHY, CHIP_ID, 29, j & 0x7F);
				}
				if (status != 0) {
					printf("[88E1111 0x] 88E1111 register dump failed.\n");
					return -1;
				}
				status |= mppa_eth_mdio_read(PHY, CHIP_ID, i, &val);
				if (status != 0) {
					printf("[88E1111 0x] 88E1111 register dump failed.\n");
					return -1;
				}
				if (val != 0)
					printf("[88E1111 0x] Register %d: 0x%.4x\n", i, val);
			}
		}
	}
	/* return to pages 0 */
	status |= mppa_eth_mdio_write(PHY, CHIP_ID, 22, 0);
	return status;
}

static void mppa_eth_mdio_synchronize()
{
	mppa_88E1111_interface_t mdio_ifce;
	mdio_ifce.mdio_master.mppa_clk_period_ps = 50000;
	mdio_ifce.chip_id = 0;
	mdio_ifce.context = (void *) (&(mdio_ifce.mdio_master));
	mdio_ifce.mppa_88E1111_read = (void *) mppa_eth_mdio_read;
	mdio_ifce.mppa_88E1111_write = (void *) mppa_eth_mdio_write;

	mppa_eth_mdio_init(mdio_ifce.chip_id);
	mppa_88E1111_configure(&mdio_ifce);
	mppa_88E1111_synchronize(&mdio_ifce);
}

#endif				/* MDIO_UTILS_H */
