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
#define PKT_SIZE	1600
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

errcode_t do_eth_open(debug_agent_interface_t *interface, int *sys_ret){
	unsigned int if_idx;
	uint32_t name, len;

	struct ifreq ethreq;
	struct sockaddr_ll sa_ll;

	ARG(0, name);
	ARG(1, len);

	char ifName[IFNAMSIZ];

	if( interface->read_dcache(interface->self, 0,  name, ifName, len) == RET_FAIL) {
		fprintf(stderr, "Error, unable to write into simulator memory \n");
		exit(1);
	}

	int fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (fd == -1) {
		goto error;
	}

	/* get if index */
	memset(&ethreq, 0, sizeof(struct ifreq));
	snprintf(ethreq.ifr_name, IFNAMSIZ, "%s", ifName);
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

errcode_t do_eth_recv_packet(debug_agent_interface_t *interface, int *sys_ret){

	uint32_t packet;
	unsigned int fd;

	ARG(0, fd);
	ARG(1, packet);

	static char buf[PKT_SIZE];


	struct sockaddr_ll sll;
	socklen_t addrlen = sizeof(sll);

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
	
	if( interface->write_memory(interface->self, 0, packet, buf, PKT_SIZE) == RET_FAIL) {
		fprintf(stderr, "Error, unable to write into simulator memory \n");
		return RET_FAIL;
	}
	interface->set_return(interface->self, 0, rz);
	return RET_OK;
}

errcode_t do_eth_send_packet(debug_agent_interface_t *interface, int *sys_ret)
{
	unsigned char packet[PKT_SIZE];
	unsigned int packet_size;
	uint32_t smem_addr;
	unsigned int fd;

	ARG(0, fd);
	ARG(1, smem_addr);
	ARG(2, packet_size);

	if( interface->read_dcache(interface->self, 0,  smem_addr, packet, packet_size) == RET_FAIL) {
		fprintf(stderr, "Error, unable to write into simulator memory \n");
		exit(1);
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


/**
 * This table holds info about syscalls.
 * For each syscall : number, number of args, return arg ? and function to call.
 */
syscall_info_ syscall_table[] = {
	{MAGIC_SCALL_ETH_INIT, 0, 0, (syscall_helper_fct) do_eth_init},
	{MAGIC_SCALL_ETH_OPEN, 2, 0, (syscall_helper_fct) do_eth_open},
	{MAGIC_SCALL_ETH_GETMAC, 2, 0, (syscall_helper_fct) do_eth_get_mac},
	{MAGIC_SCALL_ETH_RECV, 2, 0, (syscall_helper_fct) do_eth_recv_packet},
	{MAGIC_SCALL_ETH_SEND, 3, 0, (syscall_helper_fct) do_eth_send_packet},
	{0x000, 0, 0, (syscall_helper_fct) NULL} /* End of Table */
};


