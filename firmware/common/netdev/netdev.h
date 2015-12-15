#ifndef NETDEV__H
#define NETDEV__H

#include <mppa_pcie_netdev.h>

typedef struct {
	uint8_t if_id;
	uint8_t mac_addr[MAC_ADDR_LEN];	/*< Mac address */
	uint16_t mtu;
	uint32_t flags;

	uint32_t n_c2h_entries;
	uint32_t c2h_flags;

	uint32_t n_h2c_entries;
	uint32_t h2c_flags;
} eth_if_cfg_t;

extern struct mppa_pcie_eth_control eth_control;

static inline struct mppa_pcie_eth_ring_buff_desc *
netdev_get_c2h_ring_buffer(uint8_t if_id){
	return (struct mppa_pcie_eth_ring_buff_desc *)(unsigned long)
		(eth_control.configs[if_id].c2h_ring_buf_desc_addr);
}

static inline struct mppa_pcie_eth_ring_buff_desc *
netdev_get_h2c_ring_buffer(uint8_t if_id){
	return (struct mppa_pcie_eth_ring_buff_desc *)(unsigned long)
		(eth_control.configs[if_id].h2c_ring_buf_desc_addr);
}

int netdev_init(uint8_t n_if, const eth_if_cfg_t cfg[n_if]);
int netdev_init_interface(const eth_if_cfg_t *cfg);
int netdev_setup_h2c(struct mppa_pcie_eth_if_config *cfg, uint32_t n_entries,
		     uint32_t flags);
int netdev_setup_c2h(struct mppa_pcie_eth_if_config *cfg, uint32_t n_entries,
		     uint32_t flags);
int netdev_start();


#endif /* NETDEV__H */
