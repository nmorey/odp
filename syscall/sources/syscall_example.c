/**
 *  @file syscall_example.c
 *
 *  @section LICENSE
 *  Copyright (C) 2009 Kalray
 *  @author Patrice, GERIN patrice.gerin@kalray.eu
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "syscall_interface.h"
#include "debug_agent_interface.h"
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <netinet/ether.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/if_packet.h>
#include "common.h"

#define MAX_IFS     1024
#define PKT_SIZE    15000
#define MAX_IOVECS  10
#define PORTNAME	"odp%d"

struct iface {
	char name[IFNAMSIZ];
	unsigned char mac[6];
};


/* Tun structure */
static struct iface ports[MAX_IFS];

errcode_t do_eth_init(debug_agent_interface_t *interface, int *sys_ret){
	memset(ports, 0, sizeof(ports));

	interface->set_return(interface->self, 0, 0);
	return RET_OK;
}

static inline int min(int a, int b)
{
	return a <= b ? a : b;
}

errcode_t do_eth_open(debug_agent_interface_t *interface, int *sys_ret){
	unsigned int if_idx;
	uint32_t name, len;

	struct ifreq ethreq;
	struct sockaddr_ll sa_ll;

	ARG(0, name);
	ARG(1, len);
	len = min(len, IFNAMSIZ-1);	/* avoid ifName overflow */

	char ifName[IFNAMSIZ];

	if( interface->read_dcache(interface->self, 0,  name, ifName, len) == RET_FAIL) {
		fprintf(stderr, "Error, unable to write into simulator memory \n");
		exit(1);
	}
	ifName[len] = '\0';	/* len do not account for \0 */

	int fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (fd == -1) {
		goto error;
	}

	/* get if index */
	memset(&ethreq, 0, sizeof(struct ifreq));
	snprintf(ethreq.ifr_name, sizeof(ethreq.ifr_name), "%s", ifName);
	int err = ioctl(fd, SIOCGIFINDEX, &ethreq);
	if (err != 0) {
		goto error;
	}
	if_idx = ethreq.ifr_ifindex;

	/* get MAC address */
	memset(&ethreq, 0, sizeof(ethreq));
	snprintf(ethreq.ifr_name, IFNAMSIZ, "%s", ifName);
	err = ioctl(fd, SIOCGIFHWADDR, &ethreq);
	if (err != 0) {
		interface->set_return(interface->self, 0, -1);
		return RET_OK;
	}
	memcpy(ports[fd].mac, (unsigned char *)ethreq.ifr_ifru.ifru_hwaddr.sa_data, 6);

	/* bind socket to if */
	memset(&sa_ll, 0, sizeof(sa_ll));
	sa_ll.sll_family = AF_PACKET;
	sa_ll.sll_ifindex = if_idx;
	sa_ll.sll_protocol = htons(ETH_P_ALL);
	if (bind(fd, (struct sockaddr *)&sa_ll, sizeof(sa_ll)) < 0) {
		goto error;
	}

	interface->set_return(interface->self, 0, fd);
	return RET_OK;

 error:
	fprintf(stderr, "do_eth_open(%s, %i) failed:%s.\n", ifName, len, strerror(errno));
	interface->set_return(interface->self, 0, -1);
	return RET_OK;
}

errcode_t do_eth_get_mac(debug_agent_interface_t *interface, int *sys_ret){
	uint32_t fd, mac_addr;
	ARG(0, fd);
	ARG(1, mac_addr);

	if (interface->write_dcache(interface->self, 0, mac_addr, ports[fd].mac, 6) != 0) {
		fprintf(stderr, "Failed to write to data cache\n");
		exit(1);
	}
	interface->set_return(interface->self, 0, 0);
	return RET_OK;
}

typedef struct _odp_pkt_iovec {
	uint32_t iov_base;
	uint32_t iov_len;
} odp_pkt_iovec_t;

errcode_t do_eth_recv_packet(debug_agent_interface_t *interface, int *sys_ret){

	unsigned int fd;

	uint32_t smem_addr;
	odp_pkt_iovec_t iovecs[MAX_IOVECS];
	uint32_t iov_count;
	uint32_t len;
	static char buf[PKT_SIZE];

	struct sockaddr_ll sll;
	socklen_t addrlen = sizeof(sll);


	ARG(0, fd);
	ARG(1, smem_addr);
	ARG(2, iov_count);


	if (iov_count > MAX_IOVECS) {
		interface->set_return(interface->self, 0, -1);
		return RET_OK;
	}

	if( interface->read_dcache(interface->self, 0,  smem_addr, iovecs, iov_count * sizeof(odp_pkt_iovec_t)) == RET_FAIL) {
		fprintf(stderr, "Error, unable to write into simulator memory \n");
		exit(1);
	}


	ssize_t rz = recvfrom(fd, buf, PKT_SIZE, MSG_DONTWAIT,
						  (struct sockaddr *)&sll, &addrlen);
	if (rz <= 0) {
		if(errno == EAGAIN)
		{
			interface->set_return(interface->self, 0, 0);
			return RET_OK;
		}
		else
		{
			perror("read() failed");
			exit(1);
		}
	}

	uint32_t i;
	for ( i = 0, len = 0; i < iov_count && len < rz; ++i) {
		uint32_t seg_len = iovecs[i].iov_len;
		uint32_t qty = rz > seg_len ? seg_len : rz;

		if( interface->write_memory(interface->self, 0, iovecs[i].iov_base, buf + len, qty) == RET_FAIL) {
			fprintf(stderr, "Error, unable to write into simulator memory \n");
			return RET_FAIL;
		}
		len += qty;
	}

	interface->set_return(interface->self, 0, len);
	return RET_OK;
}

errcode_t do_eth_send_packet(debug_agent_interface_t *interface, int *sys_ret)
{
	unsigned char packet[PKT_SIZE];
	unsigned int packet_size;
	uint32_t smem_addr;

	odp_pkt_iovec_t iovecs[MAX_IOVECS];
	uint32_t iov_count;
	unsigned int fd;

	ARG(0, fd);
	ARG(1, smem_addr);
	ARG(2, iov_count);

	if (iov_count > MAX_IOVECS) {
		interface->set_return(interface->self, 0, -1);
		return RET_OK;
	}

	if( interface->read_dcache(interface->self, 0,  smem_addr, iovecs, iov_count * sizeof(odp_pkt_iovec_t)) == RET_FAIL) {
		fprintf(stderr, "Error, unable to write into simulator memory \n");
		exit(1);
	}

	uint32_t i;
	for( i = 0, packet_size = 0; i < iov_count; ++i){
		if( interface->read_dcache(interface->self, 0,  iovecs[i].iov_base,
					   packet + packet_size, iovecs[i].iov_len) == RET_FAIL) {
			fprintf(stderr, "Error, unable to write into simulator memory \n");
			exit(1);
		}
		packet_size += iovecs[i].iov_len;
	}

	ssize_t wz;
	do {
		wz = send(fd, packet, packet_size, 0);
	} while(wz == -1 && errno == EAGAIN);
	if (wz != packet_size) {
		perror("write() failed");
		exit(1);
	}
	interface->set_return(interface->self, 0, wz);
	return RET_OK;
}

errcode_t do_eth_promisc_set(debug_agent_interface_t *interface, int *sys_ret){
	uint32_t sockfd, enable, ret;
	ARG(0, sockfd);
	ARG(1, enable);

	struct ifreq ifr;
	snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ports[sockfd].name);

	ret = ioctl(sockfd, SIOCGIFFLAGS, &ifr);
	if (ret < 0) {
		interface->set_return(interface->self, 0, ret);
		return RET_OK;
	}

	if (enable)
		ifr.ifr_flags |= IFF_PROMISC;
	else
		ifr.ifr_flags &= ~(IFF_PROMISC);

	ret = ioctl(sockfd, SIOCSIFFLAGS, &ifr);
	interface->set_return(interface->self, 0, ret);

	return RET_OK;
}
errcode_t do_eth_promisc_get(debug_agent_interface_t *interface, int *sys_ret){
	uint32_t sockfd, ret;
	ARG(0, sockfd);
	struct ifreq ifr;

	snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ports[sockfd].name);

	ret = ioctl(sockfd, SIOCGIFFLAGS, &ifr);
	if (ret < 0) {
		interface->set_return(interface->self, 0, ret);
		return RET_OK;
	}

	interface->set_return(interface->self, 0, !!(ifr.ifr_flags & IFF_PROMISC));

	return RET_OK;
}

errcode_t do_nanosleep(debug_agent_interface_t *interface, int *sys_ret){
	uint32_t end_cycle_l, end_cycle_h;
	uint64_t end_cycle, cycles;

	ARG(0, end_cycle_l);
	ARG(1, end_cycle_h);
	end_cycle = (((uint64_t)end_cycle_h) << 32) | end_cycle_l;
	interface->read_cycle(interface->self, 0, &cycles);

	if(cycles < end_cycle){
		int timeout = end_cycle - cycles + 1;
		if(end_cycle - cycles > 0x7ffffffeULL)
			timeout = 0x7fffffff;
		interface->execution_stall(interface->self,0, timeout);
		return RET_STALL;
	}

	interface->set_return(interface->self, 0, 0);
	return RET_OK;
}
/**
 * This table holds info about syscalls.
 * For each syscall : number, number of args, return arg ? and function to call.
 */
syscall_info_ syscall_table[] = {
	{MAGIC_SCALL_ETH_INIT, 0, 0, (syscall_helper_fct) do_eth_init},
	{MAGIC_SCALL_ETH_OPEN, 2, 0, (syscall_helper_fct) do_eth_open},
	{MAGIC_SCALL_ETH_GETMAC, 2, 0, (syscall_helper_fct) do_eth_get_mac},
	{MAGIC_SCALL_ETH_RECV, 3, 0, (syscall_helper_fct) do_eth_recv_packet},
	{MAGIC_SCALL_ETH_SEND, 3, 0, (syscall_helper_fct) do_eth_send_packet},
	{MAGIC_SCALL_ETH_PROM_SET, 2, 0, (syscall_helper_fct) do_eth_promisc_set},
	{MAGIC_SCALL_ETH_PROM_GET, 1, 0, (syscall_helper_fct) do_eth_promisc_get},


	{MAGIC_SCALL_SLEEP, 1, 0, (syscall_helper_fct) do_nanosleep},
	{0x000, 0, 0, (syscall_helper_fct) NULL} /* End of Table */
};


