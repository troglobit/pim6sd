/*	$KAME: mld6.h,v 1.13 2004/06/09 15:52:57 suz Exp $	*/

/*
 * Copyright (C) 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 *  Questions concerning this software should be directed to
 *  Mickael Hoerdt (hoerdt@clarinet.u-strasbg.fr) LSIIT Strasbourg.
 *
 */
/*
 * This program has been derived from pim6dd.        
 * The pim6dd program is covered by the license in the accompanying file
 * named "LICENSE.pim6dd".
 */
/*
 * This program has been derived from pimd.        
 * The pimd program is covered by the license in the accompanying file
 * named "LICENSE.pimd".
 *
 */


#ifndef MLD6_H
#define MLD6_H

#define RECV_BUF_SIZE			64*1024
#define SO_RECV_BUF_SIZE_MAX	256*1024
#define SO_RECV_BUF_SIZE_MIN	48*1024
#define MINHLIM							1



extern int mld6_socket;
extern char *mld6_recv_buf;
extern struct sockaddr_in6 allrouters_group;
extern struct sockaddr_in6 allmld2routers_group;
extern struct sockaddr_in6 allnodes_group;
extern char *mld6_send_buf;

void init_mld6 (void);
void free_mld6 (void);
int send_mld6 (int type, int code, struct sockaddr_in6 *src,
		   struct sockaddr_in6 *dst, struct in6_addr *group,
		   int index, int delay, int datalen, int alert);
void mld_fill_msg(int ifindex, struct sockaddr_in6 *src, int alert, int datalen);

#ifndef MLD_LISTENER_QUERY
#  ifdef MLD6_LISTENER_QUERY
#    define MLD_LISTENER_QUERY		MLD6_LISTENER_QUERY
#  else
#    define MLD_LISTENER_QUERY		130
#  endif
#endif

#ifndef MLD_LISTENER_REPORT
#  ifdef MLD6_LISTENER_REPORT
#    define MLD_LISTENER_REPORT		MLD6_LISTENER_REPORT
#  else
#    define MLD_LISTENER_REPORT		131
#  endif
#endif

#ifndef MLD_LISTENER_DONE
#  ifdef MLD6_LISTENER_DONE
#    define MLD_LISTENER_DONE		MLD6_LISTENER_DONE
#  else
#    define MLD_LISTENER_DONE		132
#  endif
#endif

#ifndef MLD_MTRACE_RESP
#  ifdef MLD6_MTRACE_RESP
#    define MLD_MTRACE_RESP		MLD6_MTRACE_RESP
#  else
#    define MLD_MTRACE_RESP		200
#  endif
#endif

#ifndef MLD_MTRACE
#  ifdef MLD6_MTRACE
#    define MLD_MTRACE			MLD6_MTRACE
#  else
#    define MLD_MTRACE			201
#  endif
#endif

/* portability with older KAME headers */
#ifndef MLD_LISTENER_QUERY
#define MLD_LISTENER_QUERY	130
#define MLD_LISTENER_REPORT	131
#define MLD_LISTENER_DONE	132
#define MLD_MTRACE_RESP		201
#define MLD_MTRACE		202

#ifndef HAVE_MLD_HDR
struct mld_hdr {
	struct icmp6_hdr mld_icmp6_hdr;
	struct in6_addr	mld_addr;
};
#define mld_type mld_icmp6_hdr.icmp6_type
#define mld_code mld_icmp6_hdr.icmp6_code
#define mld_maxdelay mld_icmp6_hdr.icmp6_maxdelay

#else

#define mld_hdr		mld6_hdr
#define mld_type	mld6_type
#define mld_code	mld6_code
#define mld_maxdelay	mld6_maxdelay
#define mld_addr	mld6_addr
#endif

#define mld_cksum	mld6_cksum
#define mld_reserved	mld6_reserved
#endif

#ifndef IP6OPT_RTALERT_MLD
#define IP6OPT_RTALERT_MLD	0
#define IP6OPT_RTALERT		0x05
#endif

#ifndef IP6OPT_ROUTER_ALERT
#define IP6OPT_ROUTER_ALERT	IP6OPT_RTALERT
#endif
#ifndef IP6OPT_RTALERT_LEN
#define	IP6OPT_RTALERT_LEN	4
#endif

#ifndef IN6ADDR_LINKLOCAL_ALLNODES_INIT
#define IN6ADDR_LINKLOCAL_ALLNODES_INIT \
	{{{ 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }}}
static const struct in6_addr in6addr_linklocal_allnodes = IN6ADDR_LINKLOCAL_ALLNODES_INIT;
#endif

#endif
