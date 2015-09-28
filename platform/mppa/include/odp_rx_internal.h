/**
 * @file
 *
 * ODP packet IO - implementation internal
 */

#ifndef ODP_RX_THREAD_INTERNAL_H_
#define ODP_RX_THREAD_INTERNAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#define N_EV_MASKS 4
#define MAX_RX_IF (8 /* Eth */ + 2 /* PCI */)

typedef enum {
	RX_IF_TYPE_ETH,
	RX_IF_TYPE_PCI
} rx_if_type_e;

typedef struct {
	uint8_t pktio_id;        /**< Unique pktio [0..MAX_RX_IF[ */
	odp_pool_t pool;         /**< pool to alloc packets from */
	uint8_t dma_if;          /**< DMA Rx Interface */
	uint8_t min_port;
	uint8_t max_port;
	uint8_t header_sz;
	uint8_t pkt_offset;
	rx_if_type_e if_type;
	odp_queue_t queue;       /**< Internal queue to store
				  *   received packets */

} rx_config_t;

union mppa_ethernet_header_info_t {
	mppa_uint64 dword;
	mppa_uint32 word[2];
	mppa_uint16 hword[4];
	mppa_uint8 bword[8];
	struct {
		mppa_uint32 pkt_size : 16;
		mppa_uint32 hash_key : 16;
		mppa_uint32 lane_id  : 2;
		mppa_uint32 io_id    : 1;
		mppa_uint32 rule_id  : 4;
		mppa_uint32 pkt_id   : 25;
	} _;
};

typedef struct mppa_ethernet_header_s {
	mppa_uint64 timestamp;
	union mppa_ethernet_header_info_t info;
} mppa_ethernet_header_t;

static inline void mppa_ethernet_header_print(const mppa_ethernet_header_t *hdr)
{
	printf("EthPkt %p => {Size=%d,Hash=%d,Lane=%d,IO=%d,Rule=%03d,Id=%04d} @ %llu\n",
	       hdr,
	       hdr->info._.pkt_size,
	       hdr->info._.hash_key,
	       hdr->info._.lane_id,
	       hdr->info._.io_id,
	       hdr->info._.rule_id,
	       hdr->info._.pkt_id, hdr->timestamp);
}

int rx_thread_init(void);
int rx_thread_link_open(rx_config_t *rx_config, int n_ports, int rr_policy);
int rx_thread_link_close(uint8_t pktio_id);

#ifdef __cplusplus
}
#endif

#endif
