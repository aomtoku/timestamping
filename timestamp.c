#include <stdio.h>
#include <string.h>
#include <netinet/ether.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <linux/net_tstamp.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <linux/if_packet.h>
#include <linux/if_ether.h>

#ifndef SIOCSHWTSTAMP
# define SIOCSHWTSTAMP 0x89b0
#endif


//#define TPACKETV3

struct scm_timestamping {
	struct timespec ts[3];
};


int main (int argc, char *argv)
{
	int fd;
	int err;
	socklen_t val;
	struct ifreq device;
	struct ifreq hwtstamp;
	struct hwtstamp_config hwconfig, hwconfig_requested;
	int so_timestamping_flags = 0;
	so_timestamping_flags |= SOF_TIMESTAMPING_RX_SOFTWARE;
	so_timestamping_flags |= SOF_TIMESTAMPING_RX_HARDWARE;
	so_timestamping_flags |= SOF_TIMESTAMPING_TX_HARDWARE;
	so_timestamping_flags |= SOF_TIMESTAMPING_TX_SOFTWARE;

	fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
	
	if (fd < 0) {
		printf("fd details: %d, %d\n", fd, ETH_P_IP);
		printf("Error: cannot open socket\n");
		return -1;
	}

	/*
	 * Device Configuration
	 */
	memset(&device, 0, sizeof(device));
	strncpy(device.ifr_name, "ens255f1", sizeof(device.ifr_name));
	if (ioctl(fd, SIOCGIFADDR, &device) < 0) {
		printf("Error: ioctl\n");
		return 1;
	}
	memset(&hwtstamp, 0, sizeof(hwtstamp));
	strncpy(hwtstamp.ifr_name, "ens255f1", sizeof(hwtstamp.ifr_name));
	hwtstamp.ifr_data = (void *)&hwconfig;
	memset(&hwconfig, 0, sizeof(hwconfig));
	hwconfig.tx_type =
		(so_timestamping_flags & SOF_TIMESTAMPING_TX_HARDWARE) ?
		HWTSTAMP_TX_ON : HWTSTAMP_TX_OFF;
	hwconfig.rx_filter =
		(so_timestamping_flags & SOF_TIMESTAMPING_RX_HARDWARE) ?
		HWTSTAMP_FILTER_PTP_V1_L4_SYNC : HWTSTAMP_FILTER_NONE;
	hwconfig_requested = hwconfig;
	if (ioctl(fd, SIOCSHWTSTAMP, &hwtstamp) < 0) {
		if ((errno == EINVAL || errno == ENOTSUP) &&
		    hwconfig_requested.tx_type == HWTSTAMP_TX_OFF &&
		    hwconfig_requested.rx_filter == HWTSTAMP_FILTER_NONE)
			printf("SIOCSHWTSTAMP: disabling hardware time stamping not possible\n");
		else {
			printf("Error: SIOCSHWTSTAMP\n");
			return 1;
		}
	}
	printf("SIOCSHWTSTAMP: tx_type %d requested, got %d; rx_filter %d requested, got %d\n",
	       hwconfig_requested.tx_type, hwconfig.tx_type,
	       hwconfig_requested.rx_filter, hwconfig.rx_filter);

	
	unsigned char buf[1600];
	struct msghdr msghdr;
	int flag;
	ssize_t msglen;
	int rc;

	/*
	 * TPACKET_V3 Configuration
	 */ 
#ifdef TPACKETV3
	int v = TPACKET_V3;
	err = setsockopt(fd, SOL_PACKET, PACKET_VERSION, &v, sizeof(v));
	if (err < 0) {
		printf("Err: tpacket3 \n");
		return -1;
	}
#endif /* TPACKETV3 */

	char    inbuf[BUFSIZ];
	char    cmsgbuf[CMSG_SPACE(sizeof(struct timeval))];
	struct  cmsghdr *cmsg, *t;
	struct  iovec   msg_iov;
	struct timeval  *pTime, tv;
	const int on = 1;

	rc = setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPING, &so_timestamping_flags, sizeof(so_timestamping_flags));

	if (rc < 0) {
		printf("Error: setsockopt()\n");
		return 1;
	}

	msg_iov.iov_base = inbuf;
	msg_iov.iov_len = sizeof(inbuf);

	msghdr.msg_name = NULL;
	msghdr.msg_namelen = 0;
	msghdr.msg_iov = &msg_iov;
	msghdr.msg_iovlen = 1;
	msghdr.msg_control = cmsgbuf;
	msghdr.msg_controllen = sizeof(cmsgbuf);

	printf("Start Receiving!\n");
	while ( recvmsg(fd, &msghdr, 0) ){
		t = CMSG_FIRSTHDR(&msghdr);
		if (t != NULL) {
			printf("Success!\n");
		} 
		/* Receive auxiliary data in msgh */
		for (cmsg = CMSG_FIRSTHDR(&msghdr);
			cmsg != NULL;
			cmsg = CMSG_NXTHDR(&msghdr,cmsg)) {
			printf ("Packet coming: %x %x %x %x\n", cmsg->cmsg_level, cmsg->cmsg_type, SOL_SOCKET, SO_TIMESTAMPING);
			if (cmsg->cmsg_level == SOL_SOCKET
				&& cmsg->cmsg_type == SO_TIMESTAMPING) {
				printf("Packet comming !\n");
				pTime = (struct timeval *) CMSG_DATA(cmsg);
				printf("cmsg time=%ld,%ld\n", pTime->tv_sec, pTime->tv_usec);
				break;
			}
		}
	}
	return 0;
}
