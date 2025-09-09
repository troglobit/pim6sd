/*
 * Fred Griffoul <griffoul@ccrle.nec.de> sent me this file to use
 * it when compiling pimd under Linux.
 * There was no copyright message or author name, so I assume he was the
 * author, and deserves the copyright/credit for it: 
 *
 * COPYRIGHT/AUTHORSHIP by Fred Griffoul <griffoul@ccrle.nec.de>
 * (until proven otherwise).
 */
/*
 * $Id: netlink.c,v 1.4 2008/05/05 11:49:55 suzsuz Exp $
 */

#ifdef HAVE_CONFIG_H
#include <../include/config.h>
#endif

#ifndef HAVE_NETLINK
/* not compiled */
#else

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <strings.h>
#include <paths.h>
#include <net/if.h>
#include <string.h>
#ifdef __linux__
#include <linux/mroute6.h>
#endif
#include <syslog.h>
#include <errno.h>
#include <time.h>
#include "defs.h"
#include "vif.h"
#include "debug.h"
#include "inet6.h"

#ifdef __linux__
#include <linux/rtnetlink.h>
#endif

static int routing_socket = -1;
static __u32 seq;
pid_t pid;
int source_routing_table;

static int getmsg(struct rtmsg *rtm, int msglen, struct rpfctl *rpf);


static int addattr32(struct nlmsghdr *n, int maxlen, int type, struct in6_addr *data)
{
	struct rtattr *rta;
	int len = RTA_LENGTH(16);

	if (NLMSG_ALIGN(n->nlmsg_len) + len > maxlen)
		return -1;

	rta = (struct rtattr *)(((char *)n) + NLMSG_ALIGN(n->nlmsg_len));
	rta->rta_type = type;
	rta->rta_len = len;
	memcpy(RTA_DATA(rta), data, 16);
	n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + len;

	return 0;
}

static int parse_rtattr(struct rtattr *tb[], int max, struct rtattr *rta, int len)
{
	while (RTA_OK(rta, len)) {
		if (rta->rta_type <= max)
			tb[rta->rta_type] = rta;
		rta = RTA_NEXT(rta, len);
	}
	if (len)
		log_msg(LOG_WARNING, 0, "NETLINK: Deficit in rtattr %d\n", len);

	return 0;
}

/* open and initialize the routing socket */
int init_routesock(void)
{
	struct sockaddr_nl local;
	int addr_len;

	routing_socket = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (routing_socket < 0) {
		log_msg(LOG_ERR, errno, "netlink socket");
		return -1;
	}

	memset(&local, 0, sizeof(local));
	local.nl_family = AF_NETLINK;
	local.nl_groups = 0;
	if (bind(routing_socket, (struct sockaddr *)&local, sizeof(local)) < 0) {
		log_msg(LOG_ERR, errno, "netlink bind");
		return -1;
	}

	addr_len = sizeof(local);
	if (getsockname(routing_socket, (struct sockaddr *)&local, &addr_len) < 0) {
		log_msg(LOG_ERR, errno, "netlink getsockname");
		return -1;
	}

	if (addr_len != sizeof(local)) {
		log_msg(LOG_ERR, 0, "netlink wrong addr len");
		return -1;
	}

	if (local.nl_family != AF_NETLINK) {
		log_msg(LOG_ERR, 0, "netlink wrong addr family");
		return -1;
	}

	pid = local.nl_pid;
	seq = time(NULL);

	return 0;
}

/* get the rpf neighbor info */
int k_req_incoming(struct sockaddr_in6 *source, struct rpfctl *rpf)
{
	struct sockaddr_nl addr;
	struct nlmsghdr *n;
	struct rtmsg *r;
	char buf[512];
	int rlen;
	int l;

	rpf->source = *source;
	rpf->iif = ALL_MIFS;
	memset(&rpf->rpfneighbor, 0, sizeof(rpf->rpfneighbor));	/* initialized */

	n = (struct nlmsghdr *)buf;
	n->nlmsg_type = RTM_GETROUTE;
	n->nlmsg_flags = NLM_F_REQUEST;
	n->nlmsg_len = NLMSG_LENGTH(sizeof(*r));
	n->nlmsg_pid = pid;
	n->nlmsg_seq = ++seq;

	r = NLMSG_DATA(n);
	memset(r, 0, sizeof(*r));
	r->rtm_family = AF_INET6;
	r->rtm_dst_len = 128;
	addattr32(n, sizeof(buf), RTA_DST, &rpf->source.sin6_addr);
#ifdef CONFIG_RTNL_OLD_IFINFO
	r->rtm_optlen = n->nlmsg_len - NLMSG_LENGTH(sizeof(*r));
#endif
	r->rtm_table = source_routing_table;

	addr.nl_family = AF_NETLINK;
	addr.nl_groups = 0;
	addr.nl_pid = 0;

	/* tracef(TRF_NETLINK, "NETLINK: ask path to %s", sa6_fmt(&rpf->source)); */
	log_msg(LOG_DEBUG, 0, "NETLINK: ask path to %s", sa6_fmt(&rpf->source));

	rlen = sendto(routing_socket, buf, n->nlmsg_len, 0,
		      (struct sockaddr *)&addr, sizeof(addr));
	if (rlen < 0) {
		log_msg(LOG_WARNING, errno, "Error writing to routing socket");
		return FALSE;
	}

	do {
		int alen = sizeof(addr);

		l = recvfrom(routing_socket, buf, sizeof(buf), 0,
			     (struct sockaddr *)&addr, &alen);
		if (l < 0) {
			if (errno == EINTR)
				continue;
			log_msg(LOG_WARNING, errno, "Error writing to routing socket");
			return FALSE;
		}
	} while (n->nlmsg_seq != seq || n->nlmsg_pid != pid);

	if (n->nlmsg_type != RTM_NEWROUTE) {
		if (n->nlmsg_type != NLMSG_ERROR)
			log_msg(LOG_WARNING, 0, "netlink: wrong answer type %d",
				n->nlmsg_type);
		else
			log_msg(LOG_WARNING, -(*(int *)NLMSG_DATA(n)),
				"netlink get_route");

		return FALSE;
	}

	return getmsg(NLMSG_DATA(n), l - sizeof(*n), rpf);
}

static int getmsg(struct rtmsg *rtm, int msglen, struct rpfctl *rpf)
{
	struct rtattr *rta[RTA_MAX + 1];
	struct uvif *v;
	mifi_t vifi;

	if (rtm->rtm_type == RTN_LOCAL) {
		/* tracef(TRF_NETLINK, "NETLINK: local address"); */
		log_msg(LOG_DEBUG, 0, "NETLINK: local address");

		rpf->iif = local_address(&rpf->source);
		if (rpf->iif != NO_VIF) {
			rpf->rpfneighbor = rpf->source;
			return TRUE;
		}

		return FALSE;
	}

	memset(&rpf->rpfneighbor, 0, sizeof(rpf->rpfneighbor));	/* initialized */
	if (rtm->rtm_type != RTN_UNICAST) {
		/* tracef(TRF_NETLINK, "NETLINK: route type is %d", rtm->rtm_type); */
		log_msg(LOG_DEBUG, 0, "NETLINK: route type is %d", rtm->rtm_type);
		return FALSE;
	}

	memset(rta, 0, sizeof(rta));
	parse_rtattr(rta, RTA_MAX, RTM_RTA(rtm), msglen - sizeof(*rtm));

	if (rta[RTA_OIF]) {
		int ifindex = *(int *)RTA_DATA(rta[RTA_OIF]);

		for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
			if (v->uv_ifindex == ifindex)
				break;
		}
		if (vifi >= numvifs) {
			log_msg(LOG_WARNING, 0, "NETLINK: ifindex=%d, but no vif",
				ifindex);
			return FALSE;
		}

		/* tracef(TRF_NETLINK, "NETLINK: vif %d, ifindex=%d", vifi, ifindex); */
		log_msg(LOG_DEBUG, 0, "NETLINK: vif %d, ifindex=%d", vifi, ifindex);
	} else {
		log_msg(LOG_WARNING, 0, "NETLINK: no interface");
		return FALSE;
	}

	if (rta[RTA_GATEWAY]) {
		struct in6_addr gw;

		memcpy(&gw, RTA_DATA(rta[RTA_GATEWAY]), sizeof(gw));
		/* __u32 gw = *(__u32 *) RTA_DATA(rta[RTA_GATEWAY]); */
		/* tracef(TRF_NETLINK, "NETLINK: gateway is %s", inet6_fmt(gw)); */
		log_msg(LOG_DEBUG, 0, "NETLINK: gateway is %s", inet6_fmt(&gw));
		init_sin6(&rpf->rpfneighbor);
		rpf->rpfneighbor.sin6_addr = gw;
		rpf->rpfneighbor.sin6_scope_id = v->uv_ifindex;
	} else {
		rpf->rpfneighbor = rpf->source;
	}
	rpf->iif = vifi;

	return TRUE;
}

#endif /* HAVE_NETLINK */
