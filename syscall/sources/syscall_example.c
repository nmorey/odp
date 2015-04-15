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
#include "common.h"

#define MAX_IFS     64
#define PKT_SIZE	1600
#define PORTNAME	"odp%d"

struct iface {
	int fd;
	char name[IFNAMSIZ];
	char mac[6];
};


/* Tun structure */
static struct iface ports[MAX_IFS];
static struct pollfd pollfds[MAX_IFS];

static int tun_alloc(const char *fmt, char *name)
{
	int fd = open("/dev/net/tun", O_RDWR | O_NONBLOCK);
	if (fd < 0)	{
		perror("open(/dev/net/tun) failed");
		return -1;
	}

	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	/* Flags: IFF_TUN   - TUN device (no Ethernet headers)
	 *        IFF_TAP   - TAP device
	 *
	 *        IFF_NO_PI - Do not provide packet information
	 */
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	if (*fmt) strncpy(ifr.ifr_name, fmt, IFNAMSIZ);

	int err = ioctl(fd, TUNSETIFF, (void *)&ifr);
	if (err < 0) {
		perror("ioctl(TUNSETIFF) failed");
		close(fd);
		return err;
	}

	strcpy(name, ifr.ifr_name);
	return fd;
}

static void set_mac(char* if_name, unsigned char mac[6])
{   	
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	if (*if_name) strncpy(ifr.ifr_name, if_name, IFNAMSIZ);
	int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if(sock == -1){
	}
	memcpy(ifr.ifr_hwaddr.sa_data, mac, 6);
	ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
	if( ioctl(sock, SIOCSIFHWADDR, &ifr) )
	{
		fprintf(stderr, "socket SIOCSIFHWADDR: %s\n", strerror(errno));
		close(sock);
		exit(-1);
	}

	ifr.ifr_flags |= IFF_UP;
	ifr.ifr_flags |= IFF_RUNNING;
	if (ioctl(sock, SIOCSIFFLAGS, &ifr) < 0)  {
		fprintf(stderr, "socket SSIOCGIFFLAGS: %s\n", strerror(errno));
		close(sock);
		exit(-1);
	}
}

errcode_t do_tun_init(debug_agent_interface_t *interface, int *sys_ret){
	int i;
	memset(ports, 0, sizeof(ports));
	memset(pollfds, 0, sizeof(pollfds));

	for (i = 0; i < MAX_IFS; i++) {
		pollfds[i].fd = -1;
		pollfds[i].events = POLLIN;
		pollfds[i].revents = 0;
	}

	interface->set_return(interface->self, 0, 0);
	return RET_OK;
}

errcode_t do_tun_open(debug_agent_interface_t *interface, int *sys_ret){
	uint32_t i;
	
	ARG(0, i);

	if(i >= MAX_IFS){
		interface->set_return(interface->self, 0, -1);
		return RET_OK;
	}

	if(pollfds[i].fd != -1){
		interface->set_return(interface->self, 0, pollfds[i].fd);
		return RET_OK;
	}
	
	pollfds[i].fd = ports[i].fd = tun_alloc(PORTNAME, ports[i].name);
	if(pollfds[i].fd == -1){
		interface->set_return(interface->self, 0, -1);
		return RET_OK;
	}
	unsigned char mac[6] = { 0x12, 0x34, 0x56, 0x78, 0x00 + i / 256, 0x00 + i % 256};
	memcpy(ports[i].mac, mac, sizeof(mac));
	set_mac(ports[i].name, mac);
	
	interface->set_return(interface->self, 0, 0);
	return RET_OK;
}

errcode_t do_get_mac(debug_agent_interface_t *interface, int *sys_ret){
	uint32_t i, mac_addr;
	ARG(0, i);
	ARG(1, mac_addr);

	if(i >= MAX_IFS || pollfds[i].fd == -1){
		interface->set_return(interface->self, 0, -1);
		return RET_OK;
	}

	if (interface->write_dcache(interface->self, 0, mac_addr, ports[i].mac, 6) != 0) {
		fprintf(stderr, "Failed to write to data cache\n");
		exit(1);
	}
	interface->set_return(interface->self, 0, 0);
	return RET_OK;
}

errcode_t do_recv_packet(debug_agent_interface_t *interface, int *sys_ret){

	uint32_t packet;
	unsigned int if_id;

	ARG(0, if_id);
	ARG(1, packet);

	if(if_id >= MAX_IFS || pollfds[if_id].fd == -1){
		interface->set_return(interface->self, 0, -1);
		return RET_OK;
	}

	int fd = pollfds[if_id].fd;
	static char buf[PKT_SIZE];

	ssize_t rz = read(fd, buf, sizeof(buf));
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

errcode_t do_send_packet(debug_agent_interface_t *interface, int *sys_ret)
{
	unsigned char packet[PKT_SIZE];
	unsigned int packet_size;
	uint32_t smem_addr;
	unsigned int if_id;
	ARG(0, if_id);
	ARG(1, smem_addr);
	ARG(2, packet_size);
	printf("Wruite packet\n");
	if(if_id >= MAX_IFS || pollfds[if_id].fd == -1){
		interface->set_return(interface->self, 0, -1);
		return RET_OK;
	}

	int fd = pollfds[if_id].fd;

	if( interface->read_dcache(interface->self, 0,  smem_addr, packet, packet_size) == RET_FAIL) {
		fprintf(stderr, "Error, unable to write into simulator memory \n");
		exit(1);
	}
	ssize_t wz;
	do {
		wz = write(fd, packet, packet_size);
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
	{MAGIC_SCALL_INIT, 0, 0, (syscall_helper_fct) do_tun_init},
	{MAGIC_SCALL_OPEN, 1, 0, (syscall_helper_fct) do_tun_open},
	{MAGIC_SCALL_GETMAC, 2, 0, (syscall_helper_fct) do_get_mac},
	{MAGIC_SCALL_RECV, 2, 0, (syscall_helper_fct) do_recv_packet},
	{MAGIC_SCALL_SEND, 3, 0, (syscall_helper_fct) do_send_packet},
	{0x000, 0, 0, (syscall_helper_fct) NULL} /* End of Table */
};


