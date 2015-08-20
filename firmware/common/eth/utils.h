#ifndef UTILS_H
#define UTILS_H

#include <mppa_bsp.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <getopt.h>
#include "hal_delay.h"

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

static __inline__ void init_proc()
{

	__k1_icache_enable();
	__k1_dcache_enable();
	__k1_streaming_enable();
	__k1_hwloops_enable();

	if (__k1_is_rm()) {
		__k1_dnoc_initialize();
		__k1_cnoc_initialize(0);
		mppa_dnoc[0]->rx_global.rx_ctrl._.error_noc_fifo_full_en = 0;
		while (mppa_dnoc[0]->dma_global.error_lac.word != 0x07000000) {
		};
	}

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

static __inline__ float my_avg(uint32_t * array, uint32_t size)
{
	uint32_t i, sum = 0;
	for (i = 0; i < size; i++) {
		sum += array[i];
	}
	return sum / (float) size;
}

static __inline__ uint32_t my_min(uint32_t * array, uint32_t size)
{
	uint32_t i, min;
	min = UINT32_MAX;
	for (i = 0; i < size; i++) {
		if (min > array[i])
			min = array[i];
	}
	return min;
}

static __inline__ uint32_t my_max(uint32_t * array, uint32_t size)
{
	uint32_t i, max;
	max = 0;
	for (i = 0; i < size; i++) {
		if (max < array[i])
			max = array[i];
	}
	return max;
}

static __inline__ void my_array_display(uint32_t * array, uint32_t size)
{
	uint32_t i;
	for (i = 0; i < size; i++) {
		printf("%" PRIu32 " ", array[i]);
	}
	printf("\n");
}

static __inline__ float convert_throughput(float avg_cycles,
					   int pkt_size_in_bytes, int target_freq_in_mhz)
{
	float bits_per_cycles = (pkt_size_in_bytes * 8) / avg_cycles;
	float gigabits_per_sec = ((float) target_freq_in_mhz / 1000.0) * bits_per_cycles;
	return gigabits_per_sec;

}



static __inline__ void send_data(uint32_t interface,
				 uint32_t client_cluster_id,
				 uint64_t data_to_send, uint32_t mailbox)
{
	unsigned int route = __bsp_routing_table[__bsp_get_router_id(__k1_get_cluster_id())]
	    [__bsp_get_router_id(client_cluster_id)];

	mppa_cnoc_config_t cfg;
	cfg.dword = 0x0ULL;
	cfg._.first_dir = route & 0x7;
	mppa_cnoc_header_t hdr;
	hdr.dword = 0x0ULL;
	hdr._.route = route >> 3;
	hdr._.tag = mailbox;
	hdr._.valid = 1;

	mppa_cnoc[interface]->message_tx[0].header = hdr;
	mppa_cnoc[interface]->message_tx[0].config = cfg;
	mppa_cnoc[interface]->message_tx[0].push_eot.reg = data_to_send;
}

static __inline__ uint64_t get_data(uint32_t mailbox)
{
	uint64_t temp = __k1_umem_read64((void *)
					 &(mppa_cnoc[0]->message_ram[mailbox]));
	__k1_umem_write64((void *) &(mppa_cnoc[0]->message_ram[mailbox]), 0);
	return temp;

}

static __inline__ void sync_initiator(uint32_t interface, uint32_t mailbox,
				      uint32_t initiator_cluster_id
				      __attribute__ ((unused)),
				      uint32_t client_cluster_id, uint64_t attended_value)
{
	__k1_umem_write64((void *)
			  &(mppa_cnoc[interface]->message_ram[mailbox]), 0x0ULL);
	send_data(interface, client_cluster_id, attended_value, mailbox);
	DMSG("Waiting %d with %x\n", client_cluster_id, attended_value);
	while (__k1_umem_read64
	       ((void *) &(mppa_cnoc[interface]->message_ram[mailbox])) != attended_value) {
	};
	DMSG("Sync over with %d\n", client_cluster_id);
}

static __inline__ void sync_client(uint32_t interface, uint32_t mailbox,
				   uint32_t initiator_cluster_id,
				   uint32_t client_cluster_id
				   __attribute__ ((unused)), uint64_t attended_value)
{
	/* Waiting for the initiator */
	DMSG("Client waiting %d with %x\n", initiator_cluster_id, attended_value);
	while (__k1_umem_read64
	       ((void *) &(mppa_cnoc[interface]->message_ram[mailbox])) != attended_value) {
	};
	__k1_umem_write64((void *)
			  &(mppa_cnoc[interface]->message_ram[mailbox]), 0x0ULL);
	send_data(interface, initiator_cluster_id, attended_value, mailbox);
	/*Sending to initiator */
	DMSG("Sync over with %d\n", initiator_cluster_id);
}


static __inline__ void
send_packets(uint32_t nb_packets, uint32_t packet_size,
	     uint32_t nb_packets_per_group, uint32_t tx_chan,
	     uint32_t * cycles_sending, uint32_t cycles_to_wait)
{
	unsigned long long data_cnt = 0;
	unsigned long long header_cnt = 0;
	unsigned long long idx_in_group = 0;
	unsigned long long packet_group = 0;
	const uint32_t nb_group_packet = (nb_packets / nb_packets_per_group);

	for (header_cnt = 0, packet_group = 0; packet_group < nb_group_packet; packet_group++) {
		start_timer(0);
		for (idx_in_group = 0; idx_in_group < nb_packets_per_group;
		     idx_in_group++, header_cnt =
		     idx_in_group + packet_group * nb_packets_per_group) {
			mppa_dnoc[0]->tx_ports[tx_chan].address.word = 0xA0000000;
			for (data_cnt = 0; data_cnt < (packet_size >> 3); data_cnt++) {
				mppa_dnoc[0]->tx_ports[tx_chan].push_data.reg =
				    (header_cnt << 32) | data_cnt;
			}
			mppa_dnoc[0]->tx_ports[tx_chan].push_mask.reg = packet_size & 0x7;
			mppa_dnoc[0]->tx_ports[tx_chan].push_data_eot.reg =
			    (header_cnt << 32) | data_cnt;
			mppa_cycle_delay(cycles_to_wait);
		}
		if (cycles_sending != NULL) {
			cycles_sending[packet_group] = end_timer(0);
		}
	}
}

static __inline__ int send_single_packet(void *ptr, int len, uint32_t interface_id, uint32_t tx_id)
{
	assert(len != 0);
#ifdef DEBUG_MESSAGE
	uint32_t i;
	DMSG("Sended datas \n");
	for (i = 0; i < len; i++) {
		DMSG("%hhx ", ((uint8_t *) ptr)[i]);
	}
	DMSG("\n");
#endif

	__k1_dnoc_send_data(interface_id, tx_id, ptr, len, 1);
	return 0;
}


static inline uint32_t wait_packet(uint32_t interface_id, uint32_t rx_id)
{
	DMSG("Im' waiting in %d : %d\n", interface_id, rx_id);
	//256 RX divided in 4 registers : select the correct register           // identify the correct rx between the 63 others in this register                       
	DMSG("Event received %d != Attended %d => %llx\n",
	     (__builtin_k1_ctzdl
	      (mppa_dnoc[interface_id]->rx_global.events[rx_id >> 6].reg)),
	     __builtin_k1_ctz((0x1 << (0x3F & rx_id))),
	     ((mppa_dnoc[interface_id]->rx_global.
	       events[rx_id >> 6].reg) & (0x1ULL << (0x3FULL & rx_id))));

	while ((((mppa_dnoc[interface_id]->rx_global.
		  events[rx_id >> 6].reg) & (0x1 << (0x3F & rx_id))) == 0)) {
		//Wait for notification RX on interface interface_id //0x2 : EV1 only
		__builtin_k1_waitany(0x01000000 << interface_id, 0x2);
	}
	__k1_dnoc_event_clear1_rx(interface_id);
	return __k1_dnoc_event_cntr_load_and_clear_ext(interface_id, rx_id);
}

static inline
    int32_t wait_packet_with_timeout(uint32_t interface_id, uint32_t rx_id,
				     uint32_t cycle_before_timeout)
{

	DMSG("Im' waiting in %d : %d\n", interface_id, rx_id);
	uint32_t timer_start = get_timer(0);
	//Wrapping problems on timer
	while (timer_start + cycle_before_timeout < timer_start) {
		timer_start = get_timer(0);
	}
	//256 RX divided in 4 registers : select the correct register           // identify the correct rx between the 63 others in this register                       
	uint32_t end_timer = timer_start + cycle_before_timeout;
	while ((((mppa_dnoc[interface_id]->rx_global.
		  events[rx_id >> 6].reg) & (0x1ULL << (0x3FULL & rx_id))) == 0)) {
		uint32_t current_timer = get_timer(0);
		if (current_timer > end_timer) {
			printf
			    (" %llx 1rst Event received %u != Attended %u => %llx\n",
			     mppa_dnoc[interface_id]->rx_global.events[rx_id >> 6].reg,
			     (__builtin_k1_ctzdl
			      (mppa_dnoc[interface_id]->rx_global.events[rx_id >> 6].reg)),
			     __builtin_k1_ctz((0x1 << (0x3F & rx_id))),
			     ((mppa_dnoc[interface_id]->
			       rx_global.events[rx_id >> 6].reg) & (0x1ULL << (0x3FULL & rx_id))));
			DMSG("current = %u end_timer = %u\n", current_timer,
			     timer_start + cycle_before_timeout);
			return TIMEOUT;
		}
	}
	return __k1_dnoc_event_cntr_load_and_clear_ext(interface_id, rx_id);
}

uint64_t build_uint64(uint8_t * byte_field)
{
	uint64_t result;
	uint8_t *result_byte_filed = (void *) &result;
	uint32_t n;
	for (n = 0; n < 8; n++) {
		result_byte_filed[n] = byte_field[n];
	}
	return result;
}

static inline void my_memswap(uint8_t * first, uint8_t * second, uint32_t size)
{
	uint32_t i;
	uint8_t tmp;
	for (i = 0; i < size; i++) {
		tmp = first[i];
		first[i] = second[i];
		second[i] = tmp;
	}

}

static inline void my_memcpy(uint8_t * dest, uint8_t * src, uint32_t size)
{
	uint32_t i;
	for (i = 0; i < size; i++) {
		dest[i] = src[i];
	}
}

static inline void print_packet(uint8_t * packet, uint32_t packet_size)
{
	uint32_t i;
	for (i = 0; i < packet_size; i++) {
		if ((i % 16) == 0) {
			printf("\n%04" PRIX32 "\t", i);
		}
		printf(" %02" PRIx8, __k1_io_read8(&packet[i]));
	}
	printf("\n");
}

int32_t parse_my_mac_and_my_ip(int argc, char *argv[], uint8_t * my_mac, uint8_t * my_ip)
{
	uint32_t tmp[6];
	static struct option long_options[] = {
		{"ip", no_argument, NULL, 'i'},
		{"mac", required_argument, 0, 'm'},
		{0, 0, 0, 0}
	};
	while (argc > 1) {
		/* getopt_long stores the option index here. */
		int option_index = 0;

		int ret = getopt_long(argc, argv, "h?i:m:",
				      long_options, &option_index);

		/* Detect the end of the options. */
		if (ret == -1)
			break;

		switch (ret) {
		case 'i':
			ret = sscanf(optarg, "%lu.%lu.%lu.%lu",
				     &(tmp[0]), &(tmp[1]), &(tmp[2]), &(tmp[3]));
			if ((tmp[0] > 255) || (tmp[1] > 255)
			    || (tmp[2] > 255) || (tmp[3] > 255)) {
				printf
				    ("Please check the format of ip %lu.%lu.%lu.%lu (eg : 192.168.1.1 )\n",
				     tmp[0], tmp[1], tmp[2], tmp[3]);
				return 1;
			}
			my_ip[0] = tmp[0];
			my_ip[1] = tmp[1];
			my_ip[2] = tmp[2];
			my_ip[3] = tmp[3];
			break;

		case 'm':
			ret =
			    sscanf(optarg,
				   "%02lx:%02lx:%02lx:%02lx:%02lx:%02lx",
				   &(tmp[0]), &(tmp[1]), &(tmp[2]),
				   &(tmp[3]), &(tmp[4]), &(tmp[5]));
			if (ret != 6) {
				printf
				    ("Please check the format of arp (eg : aa:bb:cc:dd:cc:ee )\n");
				return 1;
			}

			my_mac[0] = tmp[0];
			my_mac[1] = tmp[1];
			my_mac[2] = tmp[2];
			my_mac[3] = tmp[3];
			my_mac[4] = tmp[4];
			my_mac[5] = tmp[5];
			break;

		default:
			printf("Incorrect option passed :  %s\n", argv[0]);
			return -1;
		}
	}
	return 0;

}

uint64_t route_to_cluster(uint32_t cluster_id)
{
	return __bsp_routing_table[__bsp_get_router_id(__k1_get_cluster_id())]
	    [__bsp_get_router_id(cluster_id)] >> 3;
}

uint64_t first_to_cluster(uint32_t cluster_id)
{
	return __bsp_routing_table[__bsp_get_router_id(__k1_get_cluster_id())]
	    [__bsp_get_router_id(cluster_id)] & 0x7;
}



uint8_t reverse_byte(uint8_t x)
{
	static const unsigned char table[] = {
		0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
		0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
		0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
		0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
		0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
		0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
		0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
		0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
		0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
		0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
		0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
		0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
		0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
		0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
		0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
		0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
		0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
		0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
		0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
		0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
		0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
		0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
		0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
		0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
		0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
		0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
		0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
		0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
		0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
		0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
		0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
		0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
	};
	return table[x];
}

uint16_t compute_hash(uint8_t * packet_to_send,
		      uint32_t current_packet_size
		      __attribute__ ((unused)),
		      uint16_t * current_offset_per_field,
		      uint8_t * current_hash_mask_per_field,
		      uint8_t * current_min_max_mask, uint32_t nb_fields)
{

	uint64_t extracted_values[10];
	uint8_t hash_values[80];
	int32_t i, j, l, v;
	uint8_t xor_flags;
	uint16_t crc = 0xFFFF;

	//Generate buffer of containing the value to hash
	for (i = nb_fields - 1; i >= 0; i--) {
		//Extract 64bits of the message at the correct offset
		extracted_values[i] = build_uint64(&(packet_to_send[current_offset_per_field[i]]));
		//Apply the hash_mask on the extracted value
		extracted_values[i] &= bytemask_to_bitmask(current_hash_mask_per_field[i]);
		//Insert into hash_values buffer the masked extracted values
		for (j = 7; j >= 0; j--) {
			hash_values[i * 8 + j] = ((uint8_t *) & (extracted_values[i]))[j];
		}
	}

	//for each coupled field
	for (i = nb_fields - 2; i >= 0; i -= 2) {
		uint8_t min_max_mask = current_min_max_mask[i >> 1];
		min_max_mask = reverse_byte(min_max_mask);
		if (min_max_mask != 0) {
			for (j = 7; j >= 0; j--) {
				//Could this byte  be swapped ?
				if ((min_max_mask & 0x1) != 0x0) {
					//Should this byte be swapped ?
					if (hash_values[(i * 8) + j] >
					    hash_values[((i + 1) * 8) + j]) {
						//Swap the byte in the 2 extracted word64
						my_memswap(&
							   (hash_values
							    [(i * 8) + j]),
							   &(hash_values[((i + 1) * 8) + j]), 1);
					}
				}
				//Get next byte mask
				min_max_mask >>= 1;
			}
		}
	}
	//Compute CRC16 on the hash_buffer
	for (i = nb_fields - 1; i >= 0; i--) {
		for (j = 7; j >= 0; j--) {
			v = 0x80;
			for (l = 0; l < 8; l++) {
				if ((crc & 0x8000) != 0) {
					xor_flags = 1;
				} else {
					xor_flags = 0;
				}
				crc = (crc << 1) & 0xFFFF;

				if ((hash_values[i * 8 + j] & v) != 0) {
					crc = (crc + 1) & 0xFFFF;
				}
				if (xor_flags) {
					crc = (crc ^ 0x1021) & 0xFFFF;
				}
				v = v >> 1;
			}
		}
	}
	return crc;
}

#endif				/* UTILS_H */
