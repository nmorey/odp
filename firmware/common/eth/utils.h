#ifndef UTILS_H
#define UTILS_H

#include <mppa_bsp.h>
#include <stdint.h>

#define MTU                         1600
#define MBUF_SIZE                   MTU
#define ETH_IO_MODE     2
#define CYCLE_FOR_IN_SEC 4000000
#define I2C_BITRATE      100000
#define GPIO_RATE     200000000
#define PHY				0
#define I2C				1
#define CHIP_ID			0x40
#define TIMEOUT			(0xFFFFFFFF)
#ifndef _K1_NOCV2_DECR_NOTIF_RELOAD_ETH
#define _K1_NOCV2_DECR_NOTIF_RELOAD_ETH 0x7
#endif
#define BSP_ETH_NB_FIELD_PER_RULE_DEVELOPER	10
#define BSP_ETH_NB_FIELD_PER_RULE_ETH_530	2


static inline int no_printf( __attribute__ ((unused))
			    const char *fmt, ...)
{
	return 0;
}

#ifdef DEBUG_MESSAGE
#define DMSG(fmt, ...) 				    \
    do {						        \
		printf(fmt, ##__VA_ARGS__); 	\
	} while (0)
#else
#define DMSG(fmt, ...) 				    \
    do {						        \
		no_printf(fmt, ##__VA_ARGS__); 	\
	} while (0)
#endif

#define BYTETOBINARYPATTERN "%d%d%d%d%d%d%d%d"
#define BYTETOBINARY(byte)  \
  (byte & 0x80 ? 1 : 0), \
  (byte & 0x40 ? 1 : 0), \
  (byte & 0x20 ? 1 : 0), \
  (byte & 0x10 ? 1 : 0), \
  (byte & 0x08 ? 1 : 0), \
  (byte & 0x04 ? 1 : 0), \
  (byte & 0x02 ? 1 : 0), \
  (byte & 0x01 ? 1 : 0)


static __inline__ uint32_t find_first_zero(uint8_t byte)
{
	uint8_t ret = 0;
	while ((byte & 0x1) != 0) {
		byte >>= 1;
		ret++;
	}
	return ret;
}

static __inline__ uint32_t find_first_one(uint8_t byte)
{
	uint32_t ret = 0;
	while ((byte & 0x1) == 0) {
		byte >>= 1;
		ret++;
	}
	return ret;
}

static __inline__ uint64_t bytemask_to_bitmask(uint8_t bytemask)
{
	uint64_t bitmask = 0x0ULL;
	uint32_t i;
	for (i = 0; i < 8; i++) {
		if (bytemask & 0x1) {
			bitmask |= (0xFFULL << i * 8);
		}
		bytemask >>= 1;
	}
	return bitmask;
}

static __inline__ void start_timer(uint32_t timer)
{
	__k1_counter_stop(_K1_DIAGNOSTIC_COUNTER0 + timer);
	__k1_counter_reset(_K1_DIAGNOSTIC_COUNTER0 + timer);
	__k1_counter_enable(_K1_DIAGNOSTIC_COUNTER0 + timer,
			    _K1_CYCLE_COUNT, _K1_DIAGNOSTIC_NOT_CHAINED);
}

static __inline__ uint32_t end_timer(uint32_t timer)
{

	__k1_counter_stop(_K1_DIAGNOSTIC_COUNTER0 + timer);
	return __k1_counter_num(_K1_DIAGNOSTIC_COUNTER0 + timer);
}

static __inline__ uint32_t get_timer(uint32_t timer)
{

	return __k1_counter_num(_K1_DIAGNOSTIC_COUNTER0 + timer);
}
static __inline__ uint64_t route_to_cluster(uint32_t cluster_id)
{
	return __bsp_routing_table[__bsp_get_router_id(__k1_get_cluster_id())]
	    [__bsp_get_router_id(cluster_id)] >> 3;
}

static __inline__ uint64_t first_to_cluster(uint32_t cluster_id)
{
	return __bsp_routing_table[__bsp_get_router_id(__k1_get_cluster_id())]
	    [__bsp_get_router_id(cluster_id)] & 0x7;
}

void init_proc();


float my_avg(uint32_t * array, uint32_t size);
uint32_t my_min(uint32_t * array, uint32_t size);
uint32_t my_max(uint32_t * array, uint32_t size);
void my_memswap(uint8_t * first, uint8_t * second, uint32_t size);
void my_memcpy(uint8_t * dest, uint8_t * src, uint32_t size);
uint64_t build_uint64(uint8_t * byte_field);

void my_array_display(uint32_t * array, uint32_t size);
void print_packet(uint8_t * packet, uint32_t packet_size);

float convert_throughput(float avg_cycles, int pkt_size_in_bytes,
						 int target_freq_in_mhz);
void send_data(uint32_t interface, uint32_t client_cluster_id,
			   uint64_t data_to_send, uint32_t mailbox);
uint64_t get_data(uint32_t mailbox);

void sync_initiator(uint32_t interface, uint32_t mailbox,
					uint32_t initiator_cluster_id,
					uint32_t client_cluster_id, uint64_t attended_value);
void sync_client(uint32_t interface, uint32_t mailbox,
				 uint32_t initiator_cluster_id,
				 uint32_t client_cluster_id, uint64_t attended_value);

void
send_packets(uint32_t nb_packets, uint32_t packet_size,
			 uint32_t nb_packets_per_group, uint32_t tx_chan,
			 uint32_t * cycles_sending, uint32_t cycles_to_wait);
int send_single_packet(void *ptr, int len, uint32_t interface_id, uint32_t tx_id);

uint32_t wait_packet(uint32_t interface_id, uint32_t rx_id);
int32_t wait_packet_with_timeout(uint32_t interface_id, uint32_t rx_id,
								 uint32_t cycle_before_timeout);

int32_t parse_my_mac_and_my_ip(int argc, char *argv[], uint8_t * my_mac, uint8_t * my_ip);

uint16_t compute_hash(uint8_t * packet_to_send,
					  uint32_t current_packet_size,
					  uint16_t * current_offset_per_field,
					  uint8_t * current_hash_mask_per_field,
					  uint8_t * current_min_max_mask);
#endif				/* UTILS_H */
