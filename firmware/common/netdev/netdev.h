#ifndef NETDEV__H
#define NETDEV__H

#include <mppa_pcie_netdev.h>

typedef struct {
	uint8_t if_id;
	uint8_t mac_addr[MAC_ADDR_LEN];	/*< Mac address */
	uint16_t mtu;
	uint32_t flags;

	uint32_t n_rx_entries;
	uint32_t rx_flags;

	uint32_t n_tx_entries;
	uint32_t tx_flags;
} eth_if_cfg_t;

extern struct mppa_pcie_eth_control eth_control;

static inline struct mppa_pcie_eth_ring_buff_desc *
netdev_get_rx_ring_buffer(uint8_t if_id){
	return (struct mppa_pcie_eth_ring_buff_desc *)(unsigned long)
		(eth_control.configs[if_id].rx_ring_buf_desc_addr);
}

static inline struct mppa_pcie_eth_ring_buff_desc *
netdev_get_tx_ring_buffer(uint8_t if_id){
	return (struct mppa_pcie_eth_ring_buff_desc *)(unsigned long)
		(eth_control.configs[if_id].tx_ring_buf_desc_addr);
}

int netdev_init(uint8_t n_if, const eth_if_cfg_t cfg[n_if]);
int netdev_init_interface(const eth_if_cfg_t *cfg);
int netdev_setup_tx(struct mppa_pcie_eth_if_config *cfg, uint32_t n_entries,
		    uint32_t flags);
int netdev_setup_rx(struct mppa_pcie_eth_if_config *cfg, uint32_t n_entries,
		    uint32_t flags);
int netdev_start();


#endif /* NETDEV__H */
