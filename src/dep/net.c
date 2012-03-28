/**
 * @file   net.c
 * @date   Tue Jul 20 16:17:49 2010
 * 
 * @brief  Functions to interact with the network sockets and NIC driver.
 * 
 * 
 */
#ifndef linux
#define linux
#endif

#ifdef linux
#include <linux/errqueue.h>
#include <linux/net_tstamp.h>
#include <linux/if_packet.h>
#ifndef SO_TIMESTAMPING
#define SO_TIMESTAMPING  37
#define SCM_TIMESTAMPING SO_TIMESTAMPING
#endif
#ifndef SIOCSHWTSTAMP
#define SIOCSHWTSTAMP 0x89b0
#endif
#endif /*linux*/

#include "../ptpd.h"

/* shut down the UDP stuff */

int hwtsStop(int sock, char *interface)
{
	struct ifreq hwtstamp;
	struct hwtstamp_config hwconfig;

	int so_timestamping_flags = 0;

	memset(&hwtstamp, 0, sizeof(hwtstamp));
	strncpy(hwtstamp.ifr_name, interface, sizeof(hwtstamp.ifr_name));
	hwtstamp.ifr_data = (void *)&hwconfig;

	memset(&hwconfig, 0, sizeof(hwconfig));
	hwconfig.tx_type = HWTSTAMP_TX_OFF;
	hwconfig.rx_filter = HWTSTAMP_FILTER_NONE;

	if (ioctl(sock, SIOCSHWTSTAMP, &hwtstamp) < 0) {
		PERROR("SIOCSHWTSTAMP: failed to Disable hardware time stamping");
		return FALSE;
	}


	if (setsockopt(sock, SOL_SOCKET, SO_TIMESTAMPING,
		       &so_timestamping_flags, sizeof(so_timestamping_flags)) < 0) {
		PERROR("setsockopt No SO_TIMESTAMPING");
		return FALSE;
	}

	DBG("SO_TIMESTAMPING Disabled\n");

	return TRUE;
}

Boolean 
netShutdown(NetPath * netPath)
{
	struct ip_mreq imr;
	struct ifreq ifr;

	/* Close General Multicast */
	imr.imr_multiaddr.s_addr = netPath->multicastAddr;
	imr.imr_interface.s_addr = htonl(INADDR_ANY);

	setsockopt(netPath->eventSock, IPPROTO_IP, IP_DROP_MEMBERSHIP, 
		   &imr, sizeof(struct ip_mreq));
	setsockopt(netPath->generalSock, IPPROTO_IP, IP_DROP_MEMBERSHIP, 
		   &imr, sizeof(struct ip_mreq));

	/* Close Peer Multicast */
	imr.imr_multiaddr.s_addr = netPath->peerMulticastAddr;
	imr.imr_interface.s_addr = htonl(INADDR_ANY);

	setsockopt(netPath->eventSock, IPPROTO_IP, IP_DROP_MEMBERSHIP, 
		   &imr, sizeof(struct ip_mreq));
	setsockopt(netPath->generalSock, IPPROTO_IP, IP_DROP_MEMBERSHIP, 
		   &imr, sizeof(struct ip_mreq));


	netPath->multicastAddr = 0;
	netPath->unicastAddr = 0;
	netPath->peerMulticastAddr = 0;

	if(netPath->rawSock > 0) {
		hwtsStop(netPath->rawSock, netPath->ifName);
	}
	/* Close sockets */
	if (netPath->eventSock > 0)
		close(netPath->eventSock);
	netPath->eventSock = -1;

	if (netPath->generalSock > 0)
		close(netPath->generalSock);
	netPath->generalSock = -1;

	if(netPath->rawSock > 0)
	{

		/* Raw socket, Delete 802.1AS multicast Mac address from port
		 * and then close socket.
		 */
		/* Setup Interface Request (raw socket) structure.
		 */
		bzero(&ifr, sizeof(ifr));
		strncpy((char *)ifr.ifr_name, netPath->ifName, IFNAMSIZ);
		ifr.ifr_ifindex = netPath->rawIfIndex;
		ifr.ifr_hwaddr.sa_data[0] = 0x01;
		ifr.ifr_hwaddr.sa_data[1] = 0x80;
		ifr.ifr_hwaddr.sa_data[2] = 0xC2;
		ifr.ifr_hwaddr.sa_data[3] = 0x00;
		ifr.ifr_hwaddr.sa_data[4] = 0x00;
		ifr.ifr_hwaddr.sa_data[5] = 0x0E;
		ifr.ifr_hwaddr.sa_family = AF_UNSPEC;

		if(ioctl(netPath->rawSock, SIOCDELMULTI, &ifr) < 0) {
			ERROR("SIOCDELMULTI Failed\n");
		}

		ifr.ifr_ifindex = netPath->rawIfIndex;
		ifr.ifr_hwaddr.sa_data[0] = 0x01;
		ifr.ifr_hwaddr.sa_data[1] = 0x1B;
		ifr.ifr_hwaddr.sa_data[2] = 0x19;
		ifr.ifr_hwaddr.sa_data[3] = 0x00;
		ifr.ifr_hwaddr.sa_data[4] = 0x00;
		ifr.ifr_hwaddr.sa_data[5] = 0x00;
		ifr.ifr_hwaddr.sa_family = AF_UNSPEC;

		if(ioctl(netPath->rawSock, SIOCDELMULTI, &ifr) < 0) {
			ERROR("SIOCDELMULTI Failed\n");
		}

		/* Close the raw socket */
		close(netPath->rawSock);
	}
	netPath->rawSock = -1;

	return TRUE;
}

/*Test if network layer is OK for PTP*/
UInteger8 
lookupCommunicationTechnology(UInteger8 communicationTechnology)
{
#if defined(linux)

	switch (communicationTechnology) {
	case ARPHRD_ETHER:
	case ARPHRD_EETHER:
	case ARPHRD_IEEE802:
		return PTP_ETHER;

	default:
		break;
	}

#elif defined(BSD_INTERFACE_FUNCTIONS)

#endif

	return PTP_DEFAULT;
}


 /* Find the local network interface */
UInteger32 
findIface(Octet * ifaceName, UInteger8 * communicationTechnology,
    Octet * uuid, NetPath * netPath)
{
#if defined(linux)

	/* depends on linux specific ioctls (see 'netdevice' man page) */
	int i, flags;
	struct ifconf data;
	struct ifreq device[IFCONF_LENGTH];

	data.ifc_len = sizeof(device);
	data.ifc_req = device;

	memset(data.ifc_buf, 0, data.ifc_len);

	flags = IFF_UP | IFF_RUNNING | IFF_MULTICAST;

	/* look for an interface if none specified */
	if (ifaceName[0] != '\0') {
		i = 0;
		memcpy(device[i].ifr_name, ifaceName, IFACE_NAME_LENGTH);

		if (ioctl(netPath->eventSock, SIOCGIFHWADDR, &device[i]) < 0)
			DBGV("failed to get hardware address\n");
		else if ((*communicationTechnology = 
			  lookupCommunicationTechnology(
				  device[i].ifr_hwaddr.sa_family)) 
			 == PTP_DEFAULT)
			DBGV("unsupported communication technology (%d)\n", 
			     *communicationTechnology);
		else
			memcpy(uuid, device[i].ifr_hwaddr.sa_data, 
			       PTP_UUID_LENGTH);
	} else {
		/* no iface specified */
		/* get list of network interfaces */
		if (ioctl(netPath->eventSock, SIOCGIFCONF, &data) < 0) {
			PERROR("failed query network interfaces");
			return 0;
		}
		if (data.ifc_len >= sizeof(device))
			DBG("device list may exceed allocated space\n");

		/* search through interfaces */
		for (i = 0; i < data.ifc_len / sizeof(device[0]); ++i) {
			DBGV("%d %s %s\n", i, device[i].ifr_name, 
			     inet_ntoa(((struct sockaddr_in *)
					&device[i].ifr_addr)->sin_addr));

			if (ioctl(netPath->eventSock, SIOCGIFFLAGS, 
				  &device[i]) < 0)
				DBGV("failed to get device flags\n");
			else if ((device[i].ifr_flags & flags) != flags)
				DBGV("does not meet requirements"
				     "(%08x, %08x)\n", device[i].ifr_flags, 
				     flags);
			else if (ioctl(netPath->eventSock, SIOCGIFHWADDR, 
				       &device[i]) < 0)
				DBGV("failed to get hardware address\n");
			else if ((*communicationTechnology = 
				  lookupCommunicationTechnology(
					  device[i].ifr_hwaddr.sa_family)) 
				 == PTP_DEFAULT)
				DBGV("unsupported communication technology"
				     "(%d)\n", *communicationTechnology);
			else {
				DBGV("found interface (%s)\n", 
				     device[i].ifr_name);
				memcpy(uuid, device[i].ifr_hwaddr.sa_data, 
				       PTP_UUID_LENGTH);
				memcpy(ifaceName, device[i].ifr_name, 
				       IFACE_NAME_LENGTH);
				memcpy(netPath->portMacAddress, device[i].ifr_hwaddr.sa_data, 6);
				break;
			}
		}
	}

	if (ifaceName[0] == '\0') {
		ERROR("failed to find a usable interface\n");
		return 0;
	}

	/* Get Interface Index and store into netPath if
	 * raw socket enabled
	 */
	if (netPath->rawSock != -1)
	{
		if(ioctl(netPath->rawSock, // Socket ID
				SIOCGIFINDEX,     // Socket IO Command get interface index
				&device[i]) == -1) {
			printf("findIface: get ifindex error, socket: %d, device: %s!\n",
						netPath->rawSock, device[i].ifr_name);
			return 0;
		}

		DBGV("findIface: %s, got interface index: %d\n", device[i].ifr_name,
					device[i].ifr_ifindex);
		memcpy(netPath->ifName, device[i].ifr_name, IFNAMSIZ);

		/* Store Interface Index into netPath structure for
		 * later use
		 */
		netPath->rawIfIndex = device[i].ifr_ifindex;

		/* Setup L2 Multicast address for 802.1AS and 1588 Annex F
		 * PDelay messages
		 */
		device[i].ifr_hwaddr.sa_data[0] = 0x01;
		device[i].ifr_hwaddr.sa_data[1] = 0x80;
		device[i].ifr_hwaddr.sa_data[2] = 0xC2;
		device[i].ifr_hwaddr.sa_data[3] = 0x00;
		device[i].ifr_hwaddr.sa_data[4] = 0x00;
		device[i].ifr_hwaddr.sa_data[5] = 0x0E;

		/* AKB 2010-10-07: Added fixed from Scott Atchley
		 * Need to setup sa_family as unspecified
		 */
		device[i].ifr_hwaddr.sa_family = AF_UNSPEC;

		if((ioctl(netPath->rawSock, // Socket ID
				SIOCADDMULTI,     // Socket IO Command Add multicast MAC address
				&device[i]        // Interface Request data structure
		)) == -1)     // -1 if error
		{
			printf("%d findIface: set multicast MAC error device: %s, raw socket: %d!\n", __LINE__,
					device[i].ifr_name,
					netPath->rawSock
			);
			return 0;
		}

		/* Setup L2 Multicast address for 1588 Annex F
		 * messages except PDelay
		 */
		device[i].ifr_hwaddr.sa_data[0] = 0x01;
		device[i].ifr_hwaddr.sa_data[1] = 0x1B;
		device[i].ifr_hwaddr.sa_data[2] = 0x19;
		device[i].ifr_hwaddr.sa_data[3] = 0x00;
		device[i].ifr_hwaddr.sa_data[4] = 0x00;
		device[i].ifr_hwaddr.sa_data[5] = 0x00;

		/* AKB 2010-10-07: Added fixed from Scott Atchley
		 * Need to setup sa_family as unspecified
		 */
		device[i].ifr_hwaddr.sa_family = AF_UNSPEC;

		if((ioctl(netPath->rawSock, // Socket ID
				SIOCADDMULTI,     // Socket IO Command Add multicast MAC address
				&device[i]        // Interface Request data structure
		)) == -1)     // -1 if error
		{
			printf("%d findIface: set multicast MAC error device: %s, raw socket: %d!\n", __LINE__,
					device[i].ifr_name,
					netPath->rawSock
			);
			return 0;
		}
	}
	if (ioctl(netPath->eventSock, SIOCGIFADDR, &device[i]) < 0) {
		PERROR("failed to get ip address");
		return 0;
	}
	return ((struct sockaddr_in *)&device[i].ifr_addr)->sin_addr.s_addr;

#elif defined(BSD_INTERFACE_FUNCTIONS)

	struct ifaddrs *if_list, *ifv4, *ifh;

	if (getifaddrs(&if_list) < 0) {
		PERROR("getifaddrs() failed");
		return FALSE;
	}
	/* find an IPv4, multicast, UP interface, right name(if supplied) */
	for (ifv4 = if_list; ifv4 != NULL; ifv4 = ifv4->ifa_next) {
		if ((ifv4->ifa_flags & IFF_UP) == 0)
			continue;
		if ((ifv4->ifa_flags & IFF_RUNNING) == 0)
			continue;
		if ((ifv4->ifa_flags & IFF_LOOPBACK))
			continue;
		if ((ifv4->ifa_flags & IFF_MULTICAST) == 0)
			continue;
                /* must have IPv4 address */
		if (ifv4->ifa_addr->sa_family != AF_INET)
			continue;
		if (ifaceName[0] && strncmp(ifv4->ifa_name, ifaceName, 
					    IF_NAMESIZE) != 0)
			continue;
		break;
	}

	if (ifv4 == NULL) {
		if (ifaceName[0]) {
			ERROR("interface \"%s\" does not exist,"
			      "or is not appropriate\n", ifaceName);
			return FALSE;
		}
		ERROR("no suitable interfaces found!");
		return FALSE;
	}
	/* find the AF_LINK info associated with the chosen interface */
	for (ifh = if_list; ifh != NULL; ifh = ifh->ifa_next) {
		if (ifh->ifa_addr->sa_family != AF_LINK)
			continue;
		if (strncmp(ifv4->ifa_name, ifh->ifa_name, IF_NAMESIZE) == 0)
			break;
	}

	if (ifh == NULL) {
		ERROR("could not get hardware address for interface \"%s\"\n", 
		      ifv4->ifa_name);
		return FALSE;
	}
	/* check that the interface TYPE is OK */
	if (((struct sockaddr_dl *)ifh->ifa_addr)->sdl_type != IFT_ETHER) {
		ERROR("\"%s\" is not an ethernet interface!\n", ifh->ifa_name);
		return FALSE;
	}
	DBG("==> %s %s %s\n", ifv4->ifa_name,
	    inet_ntoa(((struct sockaddr_in *)ifv4->ifa_addr)->sin_addr),
	    ether_ntoa((struct ether_addr *)
		       LLADDR((struct sockaddr_dl *)ifh->ifa_addr))
	    );

	*communicationTechnology = PTP_ETHER;
	memcpy(ifaceName, ifh->ifa_name, IFACE_NAME_LENGTH);
	memcpy(uuid, LLADDR((struct sockaddr_dl *)ifh->ifa_addr), 
	       PTP_UUID_LENGTH);

	return ((struct sockaddr_in *)ifv4->ifa_addr)->sin_addr.s_addr;

#endif
}

#ifdef linux

int hwtsInit(int sock, char *interface)
{
	struct ifreq hwtstamp;
	struct hwtstamp_config hwconfig, hwconfig_requested;

	int so_timestamping_flags = 0;
	so_timestamping_flags |= SOF_TIMESTAMPING_TX_HARDWARE;
	//so_timestamping_flags |= SOF_TIMESTAMPING_TX_SOFTWARE;		//Enabled for using SW TS
	so_timestamping_flags |= SOF_TIMESTAMPING_RX_HARDWARE;
	//so_timestamping_flags |= SOF_TIMESTAMPING_RX_SOFTWARE;		//Enabled for using SW TS
	//so_timestamping_flags |= SOF_TIMESTAMPING_SOFTWARE;		//Enabled for using SW TS
	so_timestamping_flags |= SOF_TIMESTAMPING_SYS_HARDWARE;
	so_timestamping_flags |= SOF_TIMESTAMPING_RAW_HARDWARE;

	memset(&hwtstamp, 0, sizeof(hwtstamp));
	strncpy(hwtstamp.ifr_name, interface, sizeof(hwtstamp.ifr_name));
	hwtstamp.ifr_data = (void *)&hwconfig;

	memset(&hwconfig, 0, sizeof(hwconfig));
	hwconfig.tx_type = HWTSTAMP_TX_ON;
	hwconfig.rx_filter = HWTSTAMP_FILTER_PTP_V2_EVENT;

	hwconfig_requested = hwconfig;

	if (ioctl(sock, SIOCSHWTSTAMP, &hwtstamp) >= 0) {
		DBG("SIOCSHWTSTAMP: tx_type %d requested, got %d; "
		    "rx_filter %d requested, got %d\n",
		    hwconfig_requested.tx_type, hwconfig.tx_type,
		    hwconfig_requested.rx_filter, hwconfig.rx_filter);
	} else {
		PERROR("SIOCSHWTSTAMP: failed to enable hardware time stamping");
		//return FALSE;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_TIMESTAMPING,
		       &so_timestamping_flags, sizeof(so_timestamping_flags)) < 0) {
		PERROR("setsockopt SO_TIMESTAMPING");
		return FALSE;
	}

	DBG("SO_TIMESTAMPING enabled\n");

	return TRUE;
}

static int txTimestamp(NetPath *netPath, char *pdu, int pdulen)
{
	struct cmsghdr *cmsg;
	struct iovec vec[1];
	struct msghdr msg;
	struct sock_extended_err *err;
	struct timespec *tmp, *ts = NULL;
	int cnt, index, level, matched = 0, type;
	char control[512];
	unsigned char buf[PACKET_SIZE];

	if (!netPath->hwTimestamping) {
		/* Time stamp will appear on the multicast loopback. */
		return 0;
	}

	vec[0].iov_base = buf;
	vec[0].iov_len = sizeof(buf);
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = vec;
	msg.msg_iovlen = 1;
	msg.msg_control = control;
	msg.msg_controllen = sizeof(control);

again:
	if(netPath->rawSock > 0) {
		cnt = recvmsg(netPath->rawSock, &msg, MSG_ERRQUEUE);
	} else {
		cnt = recvmsg(netPath->eventSock, &msg, MSG_ERRQUEUE);
	}
	if (cnt < 0) {
		switch (errno) {
		case EAGAIN:
		case EINTR:
			goto again;
		default:
			ERROR("recvmsg failed: %s\n", strerror(errno));
			return -1;
		}
	}
	if (cnt < pdulen) {
		ERROR("recvmsg returned only %d of %d bytes\n", cnt, pdulen);
		return -1;
	}

	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg)) {

		level = cmsg->cmsg_level;
		type  = cmsg->cmsg_type;

		if (SOL_SOCKET == level && SO_TIMESTAMPING == type) {

			if (cmsg->cmsg_len < sizeof(*tmp)*3) {
				ERROR("received short so_timestamping\n");
				return -1;
			}
			tmp = (struct timespec*)CMSG_DATA(cmsg);
			if (tmp[1].tv_sec && tmp[1].tv_nsec) {
				DBG("HW SYS Tx TIMESTAMP\n");
				ts = &tmp[1];
			}
			if (tmp[2].tv_sec && tmp[2].tv_nsec) {
				// DBG("HW RAW Tx TIMESTAMP\n");
				ts = &tmp[2];
				DBG("ts->tv_sec = %ld, ts->tv_nsec = %ld\n", ts->tv_sec, ts->tv_nsec);
			}

		} else if (IPPROTO_IP == level && IP_RECVERR == type) {

			err = (struct sock_extended_err*)CMSG_DATA(cmsg);
			if (err->ee_errno == ENOMSG &&
			    err->ee_origin == SO_EE_ORIGIN_TIMESTAMPING &&
			    !memcmp(pdu, buf + cnt - pdulen, pdulen)) {
				matched = 1;
			}
		}
	}

	if (!ts /*|| !matched*/) {
		ERROR("HW Tx TIMESTAMP Not received...\n");
		return -1;
	}

	{
		int ptype = pdu[0];
		if (netPath->rawSock > 0)
			ptype = pdu[14];
		
		ptype &= 0x0F;
		if ((ptype != SYNC) && (ptype != PDELAY_RESP)) {
			DBG("Non followup packet\n", ptype);
			return 0;
		}
	}

	/* Push packet onto stack. */
	index = netPath->tx_stack.count;
	if (TX_STACK_SIZE == index) {
		ERROR("out of stack space\n");
		return -1;
	}
	memcpy(netPath->tx_stack.data[index].buf, pdu, pdulen);
	netPath->tx_stack.data[index].len = pdulen;
	netPath->tx_stack.data[index].ts = *ts;
	netPath->tx_stack.count++;

	return 0;
}

#else /*linux*/

int hwtsInit(int sock, char *interface)
{
	return FALSE;
}

static int txTimestamp(NetPath *netPath, char *pdu, int pdulen)
{
	return 0;
}

#endif /*!linux*/

/** 
 * start all of the UDP stuff 
 * must specify 'subdomainName', and optionally 'ifaceName', 
 * if not then pass ifaceName == "" 
 * on socket options, see the 'socket(7)' and 'ip' man pages 
 *
 * @param netPath 
 * @param rtOpts 
 * @param ptpClock 
 * 
 * @return TRUE if successful
 */
Boolean 
netInit(NetPath * netPath, RunTimeOpts * rtOpts, PtpClock * ptpClock)
{
	int temp;
	struct in_addr interfaceAddr, netAddr;
	struct sockaddr_in addr;
	struct ip_mreq imr;
	char addrStr[NET_ADDRESS_LENGTH];
	struct sockaddr_ll rawaddr; /**< Link Layer raw socket data */

	DBG("netInit\n");

	/* open sockets */
	if ((netPath->eventSock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0
			|| (netPath->generalSock = socket(PF_INET, SOCK_DGRAM,
					IPPROTO_UDP)) < 0) {
		PERROR("failed to initalize sockets");
		return FALSE;
	}

	/* open raw socket if 802.1AS PTP operation is enabled */

	if (rtOpts->ethernet_mode) {
		if( (netPath->rawSock = socket(PF_PACKET, SOCK_RAW, htons(0x88F7))) < 0) {
			PERROR("netInit: failed to initalize raw socket");
			return FALSE;
		}
		DBGV("netInit: created raw socket     %d\n", netPath->rawSock);
	} else {
		/* Not 802.1AS PTP operation, make sure it isn't tried to
		 * be used later
		 */
		DBGV("netInit: raw socket create skipped\n");
		netPath->rawSock = -1;
	}

	/* find a network interface */
	if (!(interfaceAddr.s_addr = 
	      findIface(rtOpts->ifaceName, 
			&ptpClock->port_communication_technology,
			ptpClock->port_uuid_field, netPath)))
		return FALSE;

	DBG("Local IP address used : %s \n", inet_ntoa(interfaceAddr));

	temp = 1;			/* allow address reuse */
	if (setsockopt(netPath->eventSock, SOL_SOCKET, SO_REUSEADDR, 
				&temp, sizeof(int)) < 0
	    || setsockopt(netPath->generalSock, SOL_SOCKET, SO_REUSEADDR, 
	    		&temp, sizeof(int)) < 0) {
		DBG("failed to set socket reuse\n");
	}

	if(rtOpts->ethernet_mode)  {
		if(setsockopt(netPath->rawSock, SOL_SOCKET, SO_REUSEADDR,
				&temp, sizeof(int)) < 0) {
			DBG("failed to set socket reuse\n");
		}
	}

	/* bind sockets */
	/*
	 * need INADDR_ANY to allow receipt of multi-cast and uni-cast
	 * messages
	 */
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(PTP_EVENT_PORT);
	if (bind(netPath->eventSock, (struct sockaddr *)&addr, 
		 sizeof(struct sockaddr_in)) < 0) {
		PERROR("failed to bind event socket");
		return FALSE;
	}
	addr.sin_port = htons(PTP_GENERAL_PORT);
	if (bind(netPath->generalSock, (struct sockaddr *)&addr, 
		 sizeof(struct sockaddr_in)) < 0) {
		PERROR("failed to bind general socket");
		return FALSE;
	}

	if (rtOpts->ethernet_mode) {
		/* For PTP 802.1AS operation, we need to bind the raw socket */
		bzero(&rawaddr, sizeof(rawaddr));
		rawaddr.sll_family   = AF_PACKET;
#if defined(linux) || defined(__NetBSD__) || defined(__FreeBSD__)
		// Windows opens up raw sockets as regular sockets
		// and doesn't support these fields
		rawaddr.sll_ifindex  = netPath->rawIfIndex;
		rawaddr.sll_protocol = htons(0x88F7);
#else
		// TBD on what Others does here
#endif

		if(bind(netPath->rawSock,
				(struct sockaddr*)&rawaddr,
				sizeof(struct sockaddr_ll)) < 0) {
			PERROR("netInit: failed to bind raw socket");
			return FALSE;
		}
		DBGV("netInit: raw socket %d bind complete\n",
				netPath->rawSock);
		//
		// Raw socket now setup OK for 802.1AS
		// Prepare Output buffer with standard 802.1AS Multicast
		// address, the source MAC for this interface and the
		// Ethertype.  NOTE: Offsets have 2 added to them
		// so that the PTP message payload is on a 16 byte
		// aligned address.

		/* Setup L2 Mulcast address for 802.1AS */

		/* AKB NOTE: This address is the one used for all messages
		 * by 802.1AS.  IEEE 1588 specifies the default mode
		 * for direct encapsulation as two different adresses,
		 * one for path message events and one for all others.
		 * This is not supported in the current code base
		 * but may be added in a future version.
		 */
		ptpClock->outputBuffer[2] = 0x01;
		ptpClock->outputBuffer[3] = 0x80;
		ptpClock->outputBuffer[4] = 0xC2;
		ptpClock->outputBuffer[5] = 0x00;
		ptpClock->outputBuffer[6] = 0x00;
		ptpClock->outputBuffer[7] = 0x0E;

		memcpy(&ptpClock->outputBuffer[8], netPath->portMacAddress, 6);
		DBGV("Device Mac Address: %x:%x:%x:%x:%x:%x\n\n", ptpClock->outputBuffer[8] & 0xff, ptpClock->outputBuffer[9] & 0xff,
				ptpClock->outputBuffer[10] & 0xff, ptpClock->outputBuffer[11] & 0xff, ptpClock->outputBuffer[12] & 0xff,
				ptpClock->outputBuffer[13] & 0xff);

		ptpClock->outputBuffer[14] = 0x88; // Set Ethertype for PTP over 802.3 encapsulation
		ptpClock->outputBuffer[15] = 0xF7;

		//
		// AKB 2010-11-07: Setup raw addresses for either 802.1AS mode
		// (both addresses: 01-80-C2-00-00-0E) or for
		// default Annex F addressing (01-1B-19-00-00-00) for all
		// messages except PDelay messages
		//
		netPath->rawDestPDelayAddress[0] = 0x01;
		netPath->rawDestPDelayAddress[1] = 0x80;
		netPath->rawDestPDelayAddress[2] = 0xC2;
		netPath->rawDestPDelayAddress[3] = 0x00;
		netPath->rawDestPDelayAddress[4] = 0x00;
		netPath->rawDestPDelayAddress[5] = 0x0E;

		if(rtOpts->ptpAnnexF) {
			// 1588 Annex F default, use 01-1B-19-00-00-00
			// For messages other than the PDelay ones
			netPath->rawDestAddress[0] = 0x01;
			netPath->rawDestAddress[1] = 0x1B;
			netPath->rawDestAddress[2] = 0x19;
			netPath->rawDestAddress[3] = 0x00;
			netPath->rawDestAddress[4] = 0x00;
		} else {
			// 802.1AS, use same address for non PDelay messages
			// as for PDelay messages
			netPath->rawDestAddress[0] = 0x01;
			netPath->rawDestAddress[1] = 0x80;
			netPath->rawDestAddress[2] = 0xC2;
			netPath->rawDestAddress[3] = 0x00;
			netPath->rawDestAddress[4] = 0x00;
			netPath->rawDestAddress[5] = 0x0E;
		}
	} else {
		DBGV("netInit: skipping bind of raw socket\n");
	}

	/* send a uni-cast address if specified (useful for testing) */
	if (rtOpts->unicastAddress[0]) {
		/* Attempt a DNS lookup first. */
		struct hostent *host;
		host = gethostbyname2(rtOpts->unicastAddress, AF_INET);
		if (host != NULL) {
			if (host->h_length != 4) {
				PERROR("unicast host resolved to non ipv4"
				       "address");
				return FALSE;
			}
			netPath->unicastAddr = 
				*(uint32_t *)host->h_addr_list[0];
		} else {
			/* Maybe it's a dotted quad. */
			if (!inet_aton(rtOpts->unicastAddress, &netAddr)) {
				ERROR("failed to encode uni-cast address: %s\n",
				      rtOpts->unicastAddress);
				return FALSE;
				netPath->unicastAddr = netAddr.s_addr;
			}
                }
        } else
                netPath->unicastAddr = 0;
 
	/* Init General multicast IP address */
	memcpy(addrStr, DEFAULT_PTP_DOMAIN_ADDRESS, NET_ADDRESS_LENGTH);

	if (!inet_aton(addrStr, &netAddr)) {
		ERROR("failed to encode multi-cast address: %s\n", addrStr);
		return FALSE;
	}
	netPath->multicastAddr = netAddr.s_addr;

	/* multicast send only on specified interface */
	imr.imr_multiaddr.s_addr = netAddr.s_addr;
	imr.imr_interface.s_addr = interfaceAddr.s_addr;
	if (setsockopt(netPath->eventSock, IPPROTO_IP, IP_MULTICAST_IF, 
		       &imr.imr_interface.s_addr, sizeof(struct in_addr)) < 0
	    || setsockopt(netPath->generalSock, IPPROTO_IP, IP_MULTICAST_IF, 
			  &imr.imr_interface.s_addr, sizeof(struct in_addr)) 
	    < 0) {
		PERROR("failed to enable multi-cast on the interface");
		return FALSE;
	}
	/* join multicast group (for receiving) on specified interface */
	if (setsockopt(netPath->eventSock, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
		       &imr, sizeof(struct ip_mreq)) < 0
	    || setsockopt(netPath->generalSock, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
			  &imr, sizeof(struct ip_mreq)) < 0) {
		PERROR("failed to join the multi-cast group");
		return FALSE;
	}
	/* End of General multicast Ip address init */

	/* Init Peer multicast IP address */
	memcpy(addrStr, PEER_PTP_DOMAIN_ADDRESS, NET_ADDRESS_LENGTH);

	if (!inet_aton(addrStr, &netAddr)) {
		ERROR("failed to encode multi-cast address: %s\n", addrStr);
		return FALSE;
	}
	netPath->peerMulticastAddr = netAddr.s_addr;

	/* multicast send only on specified interface */
	imr.imr_multiaddr.s_addr = netAddr.s_addr;
	imr.imr_interface.s_addr = interfaceAddr.s_addr;
	if (setsockopt(netPath->eventSock, IPPROTO_IP, IP_MULTICAST_IF, 
		       &imr.imr_interface.s_addr, sizeof(struct in_addr)) < 0
	    || setsockopt(netPath->generalSock, IPPROTO_IP, IP_MULTICAST_IF, 
			  &imr.imr_interface.s_addr, sizeof(struct in_addr)) 
	    < 0) {
		PERROR("failed to enable multi-cast on the interface");
		return FALSE;
	}
	/* join multicast group (for receiving) on specified interface */
	if (setsockopt(netPath->eventSock, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
		       &imr, sizeof(struct ip_mreq)) < 0
	    || setsockopt(netPath->generalSock, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
			  &imr, sizeof(struct ip_mreq)) < 0) {
		PERROR("failed to join the multi-cast group");
		return FALSE;
	}
	/* End of Peer multicast Ip address init */


	/* set socket time-to-live to 1 */

	if (setsockopt(netPath->eventSock, IPPROTO_IP, IP_MULTICAST_TTL, 
		       &rtOpts->ttl, sizeof(int)) < 0
	    || setsockopt(netPath->generalSock, IPPROTO_IP, IP_MULTICAST_TTL, 
			  &rtOpts->ttl, sizeof(int)) < 0) {
		PERROR("failed to set the multi-cast time-to-live");
		return FALSE;
	}

	if(rtOpts->ethernet_mode) {
		netPath->hwTimestamping = hwtsInit(netPath->rawSock, rtOpts->ifaceName) ? TRUE : FALSE;
	} else {
		netPath->hwTimestamping = hwtsInit(netPath->eventSock, rtOpts->ifaceName) ? TRUE : FALSE;
	}

	/* enable loopback */
	temp = netPath->hwTimestamping ? 0 : 1;
	if (setsockopt(netPath->eventSock, IPPROTO_IP, IP_MULTICAST_LOOP, 
		       &temp, sizeof(int)) < 0
	    || setsockopt(netPath->generalSock, IPPROTO_IP, IP_MULTICAST_LOOP, 
			  &temp, sizeof(int)) < 0) {
		PERROR("failed to enable multi-cast loopback");
		return FALSE;
	}
	/* make timestamps available through recvmsg() */
	temp = netPath->hwTimestamping ? 0 : 1;
#if defined(linux) || defined(__APPLE__)
	if (setsockopt(netPath->eventSock, SOL_SOCKET, SO_TIMESTAMP, 
		       &temp, sizeof(int)) < 0
	    || setsockopt(netPath->generalSock, SOL_SOCKET, SO_TIMESTAMP, 
			  &temp, sizeof(int)) < 0) {
		PERROR("failed to enable receive time stamps");
		return FALSE;
	}

#else /* FreeBSD */
	if (setsockopt(netPath->eventSock, SOL_SOCKET, SO_BINTIME, 
		       &temp, sizeof(int)) < 0
	    || setsockopt(netPath->generalSock, SOL_SOCKET, SO_BINTIME, 
			  &temp, sizeof(int)) < 0) {
		PERROR("failed to enable receive time stamps");
		return FALSE;
	}
#endif
	return TRUE;
}

/*Check if data have been received*/
int 
netSelect(TimeInternal * timeout, NetPath * netPath)
{
	int ret, nfds;
	fd_set readfds;
	struct timeval tv, *tv_ptr;

	if (netPath->tx_stack.count)
		return TRUE;

	FD_ZERO(&readfds);
	FD_SET(netPath->eventSock, &readfds);
	FD_SET(netPath->generalSock, &readfds);

	if  (netPath->rawSock != -1)
	{
		FD_SET(netPath->rawSock, &readfds);
	}
	if (timeout) {
		tv.tv_sec = timeout->seconds;
		tv.tv_usec = timeout->nanoseconds / 1000;
		tv_ptr = &tv;
	} else
		tv_ptr = 0;

	if(netPath->eventSock > netPath->generalSock)
	{
		if (netPath->rawSock > netPath->eventSock)
		{
			nfds = netPath->rawSock;
		}
		else
		{
			nfds = netPath->eventSock;
		}
	}
	else
	{
		if (netPath->rawSock > netPath->generalSock)
		{
			nfds = netPath->rawSock;
		}
		else
		{
			nfds = netPath->generalSock;
		}
	}

	ret = select(nfds + 1, &readfds, 0, 0, tv_ptr) > 0;

	if (ret < 0) {
		if (errno == EAGAIN || errno == EINTR)
			return 0;
	}
	return ret;
}

static int do_rx_timestamping(struct msghdr *msg, TimeInternal *time)
{
	struct cmsghdr *cm;
	struct timeval *tv = NULL;

	for (cm = CMSG_FIRSTHDR(msg); cm != NULL; cm = CMSG_NXTHDR(msg, cm)) {

		struct timespec *stamp;

		if (cm->cmsg_level != SOL_SOCKET)
			continue;

		switch (cm->cmsg_type) {

		case SCM_TIMESTAMP:
			tv = (struct timeval *)CMSG_DATA(cm);
			time->seconds = tv->tv_sec;
			time->nanoseconds = tv->tv_usec*1000;
			break;

		case SO_TIMESTAMPING:
			/* array of three time stamps: software, HW, raw HW */
			stamp = (struct timespec*)CMSG_DATA(cm);

			if (cm->cmsg_len < sizeof(*stamp)*3) {
				ERROR("received short SO_TIMESTAMPING (%d/%d)\n",
				      cm->cmsg_len, (int)sizeof(*stamp)*3);
				return 0;
			}

			/* look at second element in array which is the HW tstamp */
			stamp++;
			if (stamp->tv_sec && stamp->tv_nsec) {
				DBG("HW SYS Rx TIMESTAMP\n");
				time->seconds = stamp->tv_sec;
				time->nanoseconds = stamp->tv_nsec;
				tv = (struct timeval*)1;
				break;
			}

			/* No SYS HW time stamp, look for a RAW HW time stamp. */
			stamp++;
			if (stamp->tv_sec && stamp->tv_nsec) {
				DBG("HW RAW Rx TIMESTAMP\n");
				time->seconds = stamp->tv_sec;
				time->nanoseconds = stamp->tv_nsec;
				tv = (struct timeval*)1;
				break;
			}

			ERROR("Time stamping for Rx Not received!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

		break;

		}
	}

	if (!tv) {
		DBG("no receive time stamp\n");
		return -1;
	}
	DBG("kernel recv time stamp %us %dns\n", time->seconds, time->nanoseconds);
	return 0;
}

/** 
 * store received data from network to "buf" , get and store the
 * SO_TIMESTAMP value in "time" for an event message
 * 
 * @param buf 
 * @param time 
 * @param netPath 
 * 
 * @return 
 */
ssize_t 
netRecvEvent(Octet * buf, TimeInternal * time, NetPath * netPath)
{
	ssize_t ret;
	struct msghdr msg;
	struct iovec vec[1];
	struct sockaddr_in from_addr;

	union {
		struct cmsghdr cm;
		char control[512];
	}     cmsg_un;

#ifndef linux
	struct cmsghdr *cmsg;
#if defined(__APPLE__)
	struct timeval *tv;
#else
	struct timespec ts;
#endif
#endif

	/* Pop packet from stack. */
	if (netPath->tx_stack.count) {
		int index = netPath->tx_stack.count - 1;
		ret = netPath->tx_stack.data[index].len;
		memcpy(buf, netPath->tx_stack.data[index].buf, ret);
		time->seconds = netPath->tx_stack.data[index].ts.tv_sec;
		time->nanoseconds = netPath->tx_stack.data[index].ts.tv_nsec;
		netPath->tx_stack.count--;
		return ret;
	}

	vec[0].iov_base = buf;
	vec[0].iov_len = PACKET_SIZE;

	memset(&msg, 0, sizeof(msg));
	memset(&from_addr, 0, sizeof(from_addr));
	memset(buf, 0, PACKET_SIZE);
	memset(&cmsg_un, 0, sizeof(cmsg_un));

	msg.msg_name = (caddr_t)&from_addr;
	msg.msg_namelen = sizeof(from_addr);
	msg.msg_iov = vec;
	msg.msg_iovlen = 1;
	msg.msg_control = cmsg_un.control;
	msg.msg_controllen = sizeof(cmsg_un.control);
	msg.msg_flags = 0;

	ret = recvmsg(netPath->eventSock, &msg, MSG_DONTWAIT);
	if (ret <= 0) {
		if (errno == EAGAIN || errno == EINTR)
			return 0;

		return ret;
	}
	if (msg.msg_flags & MSG_TRUNC) {
		ERROR("received truncated message\n");
		return 0;
	}
	/* get time stamp of packet */
	if (!time) {
		ERROR("null receive time stamp argument\n");
		return 0;
	}
	if (msg.msg_flags & MSG_CTRUNC) {
		ERROR("received truncated ancillary data\n");
		return 0;
	}

#ifdef linux
	return do_rx_timestamping(&msg, time) ? 0 : ret;
#else
#if defined(__APPLE__)
	tv = 0;
	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; 
	     cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_level == SOL_SOCKET && 
		    cmsg->cmsg_type == SCM_TIMESTAMP)
			tv = (struct timeval *)CMSG_DATA(cmsg);
	}

	if (tv) {
		time->seconds = tv->tv_sec;
		time->nanoseconds = tv->tv_usec * 1000;
		DBGV("kernel recv time stamp %us %dns\n", 
		     time->seconds, time->nanoseconds);
#else /* FreeBSD has more accurate time stamps */
	bzero(&ts, sizeof(ts));
	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; 
	     cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_level == SOL_SOCKET && 
		    cmsg->cmsg_type == SCM_BINTIME)
			bintime2timespec((struct bintime *)CMSG_DATA(cmsg),
					 &ts);
	}

	if (ts.tv_sec != 0) {
		time->seconds = ts.tv_sec;
		time->nanoseconds = ts.tv_nsec;
		DBGV("kernel recv time stamp %us %dns\n", time->seconds, time->nanoseconds);
#endif /* Linux or FreeBSD */
	} else {
		/*
		 * do not try to get by with recording the time here, better
		 * to fail because the time recorded could be well after the
		 * message receive, which would put a big spike in the
		 * offset signal sent to the clock servo
		 */
		DBG("no receive time stamp\n");
		return 0;
	}

	return ret;
#endif
}



/** 
 * 
 * store received data from network to "buf" get and store the
 * SO_TIMESTAMP value in "time" for a general message
 * 
 * @param buf 
 * @param time 
 * @param netPath 
 * 
 * @return 
 */

ssize_t 
netRecvGeneral(Octet * buf, TimeInternal * time, NetPath * netPath)
{
	ssize_t ret;
	struct sockaddr_in from_addr;
	socklen_t addr_len = sizeof(struct sockaddr_in);

	ret = recvfrom(netPath->generalSock, buf, PACKET_SIZE, MSG_DONTWAIT,
		       (struct sockaddr *)&from_addr, &addr_len);
	if (ret <= 0) {
		if (errno == EAGAIN || errno == EINTR)
			return 0;

		return ret;
	}

	return ret;
}

#if 1
ssize_t
netRecvRaw(Octet * buf, TimeInternal * time, NetPath * netPath)
{
	ssize_t ret;
	struct msghdr msg;
	struct iovec vec[1];
	struct sockaddr_in from_addr;

	union {
		struct cmsghdr cm;
		char control[512];
	}     cmsg_un;

	/* Pop packet from stack. */
	if (netPath->tx_stack.count) {
		int index = netPath->tx_stack.count - 1;

		DBGV("Receiving Packet for Follow Up...\n");
		ret = netPath->tx_stack.data[index].len;
		memcpy(buf, netPath->tx_stack.data[index].buf, ret);
		time->seconds = netPath->tx_stack.data[index].ts.tv_sec;
		time->nanoseconds = netPath->tx_stack.data[index].ts.tv_nsec;
		netPath->tx_stack.count--;
		return ret;
	}

	vec[0].iov_base = buf;
	vec[0].iov_len = PACKET_SIZE;

	memset(&msg, 0, sizeof(msg));
	memset(&from_addr, 0, sizeof(from_addr));
	memset(buf, 0, PACKET_SIZE);
	memset(&cmsg_un, 0, sizeof(cmsg_un));

	msg.msg_name = (caddr_t)&from_addr;
	msg.msg_namelen = sizeof(from_addr);
	msg.msg_iov = vec;
	msg.msg_iovlen = 1;
	msg.msg_control = cmsg_un.control;
	msg.msg_controllen = sizeof(cmsg_un.control);
	msg.msg_flags = 0;

	ret = recvmsg(netPath->rawSock, &msg, MSG_DONTWAIT);
	if (ret <= 0) {
		if (errno == EAGAIN || errno == EINTR)
			return 0;
		DBGV("Received Error message... ret = %d\n", ret);
		return ret;
	}
	if (msg.msg_flags & MSG_TRUNC) {
		ERROR("received truncated message\n");
		return 0;
	}
	/* get time stamp of packet */
	if (!time) {
		ERROR("null receive time stamp argument\n");
		return 0;
	}
	if (msg.msg_flags & MSG_CTRUNC) {
		ERROR("received truncated ancillary data\n");
		return 0;
	}

	DBGV(">>>>>>>>>>>>>>>>>>>>>start do_rx_timestamping... received = %d\n", ret);
#ifdef linux
	return do_rx_timestamping(&msg, time) ? 0 : ret;
#else
	return ret;
#endif
}

#else
/**
 * Function to receive a PTP direct ethernet encapsulation
 * from a raw socket
 */
ssize_t netRecvRaw(Octet *buf, NetPath *netPath)
{
	ssize_t ret;
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(struct sockaddr_in);

	DBGV("netRecvRaw:     %s calling recvfrom, socket: %d\n",
			netPath->ifName,
			netPath->rawSock);

	ret = recvfrom(netPath->rawSock,
			buf,
			PACKET_SIZE,
			MSG_DONTWAIT,
			(struct sockaddr *)&addr,
			&addr_len);

	if(ret <= 0)
	{
		if(errno == EAGAIN || errno == EINTR)
		{
			DBGV("netRecvRaw:     No message (EAGAIN or EINTR)\n");
			return 0;
		}

		DBGV("netRecvRaw:     recvfrom error : %d\n", ret);
		return ret;
	}

	DBGV("netRecvRaw:     %s length: %d\n", netPath->ifName, ret);
#ifdef linux
	return do_rx_timestamping(&msg, time) ? 0 : ret;
#endif
	return ret;
}
#endif

ssize_t 
netSendEvent(Octet * buf, UInteger16 length, NetPath * netPath)
{
	ssize_t ret;
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(PTP_EVENT_PORT);

	if (netPath->unicastAddr) {
		addr.sin_addr.s_addr = netPath->unicastAddr;

		ret = sendto(netPath->eventSock, buf, length, 0, 
			     (struct sockaddr *)&addr, 
			     sizeof(struct sockaddr_in));
		if (ret <= 0)
			DBG("error sending uni-cast event message\n");
		/* 
		 * Need to forcibly loop back the packet since
		 * we are not using multicast. 
		 */
		addr.sin_addr.s_addr = htonl(0x7f000001);

		ret = sendto(netPath->eventSock, buf, length, 0, 
			     (struct sockaddr *)&addr, 
			     sizeof(struct sockaddr_in));
		if (ret <= 0)
			DBG("error looping back uni-cast event message\n");
		
	} else {
		addr.sin_addr.s_addr = netPath->multicastAddr;

		ret = sendto(netPath->eventSock, buf, length, 0, 
			     (struct sockaddr *)&addr, 
			     sizeof(struct sockaddr_in));
		if (ret <= 0)
			DBG("error sending multi-cast event message\n");
	}

	if (txTimestamp(netPath, buf, length)) {
		ERROR("txTimestamp failed\n");
		exit(1);
	}

	return ret;
}

ssize_t 
netSendGeneral(Octet * buf, UInteger16 length, NetPath * netPath)
{
	ssize_t ret;
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(PTP_GENERAL_PORT);

	if (netPath->unicastAddr) {
		addr.sin_addr.s_addr = netPath->unicastAddr;

		ret = sendto(netPath->eventSock, buf, length, 0, 
			     (struct sockaddr *)&addr, 
			     sizeof(struct sockaddr_in));
		if (ret <= 0)
			DBG("error sending uni-cast general message\n");
	} else {
		addr.sin_addr.s_addr = netPath->multicastAddr;

		ret = sendto(netPath->generalSock, buf, length, 0, 
			     (struct sockaddr *)&addr, 
			     sizeof(struct sockaddr_in));
		if (ret <= 0)
			DBG("error sending multi-cast general message\n");
	}
	return ret;
}

ssize_t 
netSendPeerGeneral(Octet * buf, UInteger16 length, NetPath * netPath)
{

	ssize_t ret;
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(PTP_GENERAL_PORT);

	if (netPath->unicastAddr) {
		addr.sin_addr.s_addr = netPath->unicastAddr;

		ret = sendto(netPath->eventSock, buf, length, 0, 
			     (struct sockaddr *)&addr, 
			     sizeof(struct sockaddr_in));
		if (ret <= 0)
			DBG("error sending uni-cast general message\n");

	} else {
		addr.sin_addr.s_addr = netPath->peerMulticastAddr;

		ret = sendto(netPath->generalSock, buf, length, 0, 
			     (struct sockaddr *)&addr, 
			     sizeof(struct sockaddr_in));
		if (ret <= 0)
			DBG("error sending multi-cast general message\n");
	}
	return ret;

}

/**
 * Functon to send a PTP direct ethernet encapsulation message
 * to a raw socket
 */
ssize_t netSendRaw(Octet *buf, UInteger16 length, NetPath *netPath, Boolean pdelay)
{
	ssize_t ret;
	struct  sockaddr_ll rawaddr;

	DBGV("netSendRaw: buf %p, length: %d\n", buf, length);

	/* Dump hex data if message level debugging enabled */
	if (pdelay)
	{
		/* PDelay message, send to default peer delay message
		 * multicast MAC address
		 */
		buf[0] = netPath->rawDestPDelayAddress[0];
		buf[1] = netPath->rawDestPDelayAddress[1];
		buf[2] = netPath->rawDestPDelayAddress[2];
		buf[3] = netPath->rawDestPDelayAddress[3];
		buf[4] = netPath->rawDestPDelayAddress[4];
		buf[5] = netPath->rawDestPDelayAddress[5];
	}
	else
	{
		buf[0] = netPath->rawDestAddress[0];
		buf[1] = netPath->rawDestAddress[1];
		buf[2] = netPath->rawDestAddress[2];
		buf[3] = netPath->rawDestAddress[3];
		buf[4] = netPath->rawDestAddress[4];
		buf[5] = netPath->rawDestAddress[5];
	}

#ifdef DBGM_ENABLED
	{
		int i;
		for (i=0; i<length; i++)
		{
			if (i % 16 == 0)
			{
				fprintf(stderr, "\nnetSendRaw:     ");
				fprintf(stderr, "%4.4x:", i);
			}
			fprintf(stderr, " %2.2x", buf[i] & 0xff);
		}
		fprintf(stderr,"\n\n");
	}
#endif
	/* */

	bzero(&rawaddr, sizeof(rawaddr));
	rawaddr.sll_family   = AF_PACKET;

	// Windows raw sockets uses normal sockets, TDB on how to or if
	// we need to support these fields
	rawaddr.sll_ifindex  = netPath->rawIfIndex;
	rawaddr.sll_protocol = htons(0x88F7);

	ret = sendto(netPath->rawSock,
			buf,
			length,
			0,
			(struct sockaddr *)&rawaddr,
			sizeof(struct sockaddr_ll)
	);
	if(ret <= 0)
	{
		DBG("netSendRaw: error %d sending raw frame\n", ret);
		return ret;
	}

	DBGV("netSendRaw: %s requested:%d, sent:%d\n",
			netPath->ifName,
			length,
			ret);

	if (txTimestamp(netPath, buf, length)) {
		ERROR("txTimestamp failed\n");
		exit(1);
	}

	return ret;
}

ssize_t 
netSendPeerEvent(Octet * buf, UInteger16 length, NetPath * netPath)
{
	ssize_t ret;
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(PTP_EVENT_PORT);

	if (netPath->unicastAddr) {
		addr.sin_addr.s_addr = netPath->unicastAddr;

		ret = sendto(netPath->eventSock, buf, length, 0, 
			     (struct sockaddr *)&addr, 
			     sizeof(struct sockaddr_in));
		if (ret <= 0)
			DBG("error sending uni-cast event message\n");
	} else {
		addr.sin_addr.s_addr = netPath->peerMulticastAddr;

		ret = sendto(netPath->eventSock, buf, length, 0, 
			     (struct sockaddr *)&addr, 
			     sizeof(struct sockaddr_in));
		if (ret <= 0)
			DBG("error sending multi-cast event message\n");
	}
	return ret;
}
