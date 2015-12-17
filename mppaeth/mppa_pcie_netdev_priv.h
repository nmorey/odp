#ifndef MPPA_PCIE_ETH_PRIV_H
#define MPPA_PCIE_ETH_PRIV_H

#define MPPA_PCIE_NETDEV_NAPI_WEIGHT  16

/* Sufficient for K1B not for K1A but not expected to be used */
#define MPPA_PCIE_NETDEV_NOC_CHAN_COUNT 4

#define SMEM_BAR_VADDR(__pdata) __pdata->bar[__pdata->smem_bar].vaddr

#define desc_info_addr(pdata, addr, field)				\
	SMEM_BAR_VADDR(pdata) + addr +  offsetof(struct mppa_pcie_eth_ring_buff_desc, field)

enum _mppa_pcie_netdev_state {
	_MPPA_PCIE_NETDEV_STATE_DISABLED = 0,
	_MPPA_PCIE_NETDEV_STATE_ENABLING = 1,
	_MPPA_PCIE_NETDEV_STATE_ENABLED = 2,
	_MPPA_PCIE_NETDEV_STATE_DISABLING = 3,
};

struct mppa_pcie_netdev_tx {
	struct sk_buff *skb;
	struct scatterlist sg[MAX_SKB_FRAGS + 1];
	u32 sg_len;
	dma_cookie_t cookie;
	void *entry_addr;
	u32 len; /* to be able to free the skb in the TX 2nd step */
	struct timespec time;
	dma_addr_t dst_addr;
	int chanidx;
	uint32_t flags;
};

struct mppa_pcie_netdev_rx {
	struct sk_buff *skb;
	struct scatterlist sg[1];
	u32 sg_len;
	dma_cookie_t cookie;
	void *entry_addr;
	u32 len; /* avoid to re-read the entry in the RX 2nd step */
};

struct mppa_pcie_netdev_priv {
	struct napi_struct napi;

	struct mppa_pcie_device *pdata;

	struct dentry *dir;

	struct mppa_pcie_eth_if_config *config;
	struct net_device *netdev;

	int schedule_napi;

	atomic_t reset;

	int interrupt_status;
	u8 __iomem *interrupt_status_addr;

	/* TX ring */
	struct dma_chan *tx_chan[MPPA_PCIE_NETDEV_NOC_CHAN_COUNT+1];
	struct mppa_pcie_dma_slave_config tx_config[MPPA_PCIE_NETDEV_NOC_CHAN_COUNT+1];
	struct mppa_pcie_netdev_tx *tx_ring;
	atomic_t tx_done;
	atomic_t tx_tail;
	u8 __iomem *tx_tail_addr;
	atomic_t tx_head;
	u8 __iomem *tx_head_addr;
	int tx_size;
	int tx_cached_head;

	/* RX ring */
	struct dma_chan *rx_chan;
	struct mppa_pcie_dma_slave_config rx_config;
	struct mppa_pcie_netdev_rx *rx_ring;
	int rx_used;
	int rx_avail;
	int rx_tail;
	u8 __iomem *rx_tail_addr;
	int rx_head;
	u8 __iomem *rx_head_addr;
	int rx_size;
	struct mppa_pcie_eth_c2h_ring_buff_entry *rx_mppa_entries;

	struct mppa_pcie_time *tx_time;
};

struct mppa_pcie_pdata_netdev {
	struct mppa_pcie_device *pdata;
	atomic_t state;
	struct net_device *dev[MPPA_PCIE_ETH_MAX_INTERFACE_COUNT];
	int if_count;
	struct mppa_pcie_eth_control control;
	struct work_struct enable; /* cannot register in interrupt context */
};

#endif
