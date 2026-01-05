/*
 * $KAME: mld6v2.c,v 1.21 2005/05/19 08:11:26 suz Exp $
 */

/*
 * Copyright (C) 1999 LSIIT Laboratory.
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
 * This program has been derived from pimd.
 * The pimd program is covered by the license in the accompanying file
 * named "LICENSE.pimd".
 *
 */

#include "defs.h"
#include "vif.h"
#include "debug.h"
#include "inet6.h"
#include "mld6.h"
#include "mld6v2.h"
#include "mld6_proto.h"
#include "mld6v2_proto.h"
#include "callout.h"
#include "timer.h"

#ifdef HAVE_MLDV2

/* uses buffer in mld6.c */
extern char    *sndcmsgbuf;
extern int      ctlbuflen;
extern struct iovec sndiov[2];
extern struct msghdr sndmh;

static struct sockaddr_in6 dst_sa;
static int make_mld6v2_msg(int type, int code, struct sockaddr_in6 *src,
			   struct sockaddr_in6 *dst,
			   struct sockaddr_in6 *group, int ifindex,
			   unsigned int delay, int datalen, int alert,
			   int sflag, int qrv, int qqic, int gss);

/*
 * this function build three type of messages : 
 *	- general queries
 *	- group spec. query S flag set/not set
 *	- source/group spec. query S flag set
 *	- source/group spec. queries S flag not set
 *
 * specific queries are created in the following manner:
 *	- clear S flag
 *	- look up the source list of the group 
 *	- send a specific query with all the sources satisfying either
 *	  of the two conditions
 *		* state == LESSTHANLLQI && this is a specific query
 *		* state == MORETHANLLQI 
 * 	- set S flag
 *	if there something to send, return TRUE.
 * initialisation, and reading are done in mld6.c (it's just another icmp6 filtering) 
 * only used for mldv2 query messages 
 */

static int
make_mld6v2_msg(int type, int code, struct sockaddr_in6 *src,
		struct sockaddr_in6 *dst, struct sockaddr_in6 *group,
		int ifindex,
		unsigned int delay, int datalen, int alert, int sflag,
		int qrv, int qqic, int gss)
{
    struct mldv2_hdr *mhp = (struct mldv2_hdr *) mld6_send_buf;
    int             nbsrc = 0;
    struct listaddr *lstsrc = NULL;
    u_int8_t        misc = 0;	/*Resv+S flag + QRV */
    unsigned int    realnbr;
    mifi_t vifi;
    struct uvif    *v;
    struct listaddr *g = NULL;

    init_sin6(&dst_sa);
    dst_sa.sin6_addr = allnodes_group.sin6_addr;
    sndmh.msg_name = (caddr_t) &dst_sa;

    if ((vifi = local_address(src)) == NO_VIF) {
        IF_DEBUG(DEBUG_MLD)
            log_msg(LOG_INFO, 0, "make_mld6v2_msg: can't find a vif");
        return FALSE;
    }
    v = &uvifs[vifi];

    if (group != NULL) {
        lstsrc = check_multicastV2_listener(v, group, &g, NULL);
        if (g == NULL) {
            IF_DEBUG(DEBUG_MLD)
                log_msg(LOG_WARNING, 0,
                   "trying to build group specific query without state for it");
            return FALSE;
        }
	dst_sa.sin6_addr = group->sin6_addr;
    }

    /* scan the source-list only in case of GSS query */
    if (gss) {
        if (sflag == SFLAGNO) {
            lstsrc = g->sources;
            while (lstsrc) {
                /*
                 * Section 6.6.1 draft-vida-mld-v2-00.txt : When a router send 
		 * a query with clear router Side Processing flag,
                 * it must update its timer to reflect the correct timeout 
                 * values: source timer for sources are lowered to LLQI
                 */
                if (lstsrc->al_checklist != LESSTHANLLQI)
		    goto nextsrc;
                if (lstsrc->al_rob <= 0)
		    goto nextsrc;	/* the source will be deleted */
                IF_DEBUG(DEBUG_MLD)
                    log_msg(LOG_DEBUG, 0, "%s", sa6_fmt(&lstsrc->al_addr));

                mhp->mld_src[nbsrc] = lstsrc->al_addr.sin6_addr;
                nbsrc++;
                lstsrc->al_rob--;
                if (timer_leftTimer(lstsrc->al_timerid) >
                    MLD6_LAST_LISTENER_QUERY_INTERVAL / MLD6_TIMER_SCALE) {
                         timer_clearTimer(lstsrc->al_timerid);
                         SET_TIMER(lstsrc->al_timer,
                                   MLD6_LAST_LISTENER_QUERY_INTERVAL /
                                   MLD6_TIMER_SCALE);
                         lstsrc->al_timerid = SetTimerV2(vifi, g, lstsrc);
                }
	    nextsrc:
                lstsrc = lstsrc->al_next;
            }
        }
        if (sflag == SFLAGYES) {
            lstsrc = g->sources;
            while (lstsrc) {
                if (lstsrc->al_checklist != MORETHANLLQI)
		    goto nextsrc2;

                if (lstsrc->al_rob <= 0) {
                    lstsrc->al_checklist = FALSE;
		    goto nextsrc2;
		}

                IF_DEBUG(DEBUG_MLD)
                    log_msg(LOG_DEBUG, 0, "%s", sa6_fmt(&lstsrc->al_addr));
                mhp->mld_src[nbsrc] = lstsrc->al_addr.sin6_addr;
                nbsrc++;
                lstsrc->al_rob--;
	    nextsrc2:
                lstsrc = lstsrc->al_next;
            }
        }
	if (nbsrc == 0) {
		log_msg(LOG_DEBUG, 0, "No source found for this GSS Query"
			"(%s S-flag %s)",
			sa6_fmt(group), sflag == SFLAGYES ? "ON" : "OFF");
		return FALSE;
	}
    }

    IF_DEBUG(DEBUG_MLD) {
	log_msg(LOG_DEBUG, 0, "==>(%s) Query Sent With S flag %s",
		sa6_fmt(&dst_sa), sflag == SFLAGYES ? "ON" : "OFF");
    }

    /* fill the misc field */
    misc |= sflag;
    if (qrv <= 7)
	misc |= qrv;

    /* XXX : hard-coding, 28 is the minimal size of the mldv2 query header */
    datalen = 28 + nbsrc * sizeof(struct in6_addr);
    mhp->mld_type = type;
    mhp->mld_code = code;
    mhp->mld_maxdelay = htons(codafloat(delay, &realnbr, 3, 12));
    if (group!=NULL)
	mhp->mld_addr = group->sin6_addr;
    else
	mhp->mld_addr = in6addr_any;
    mhp->mld_rtval = misc;
    mhp->mld_qqi = codafloat(qqic, &realnbr, 3, 4);
    mhp->mld_numsrc = htons(nbsrc);

    mld_fill_msg(ifindex, src, alert, datalen);

    return TRUE;
}

int
send_mld6v2(int type, int code, struct sockaddr_in6 *src,
	    struct sockaddr_in6 *dst, struct sockaddr_in6 *group, int index,
	    unsigned int delay, int datalen, int alert, int sflag, int qrv,
	    int qqic, int gss)
{
    struct sockaddr_in6 *dstp;

    if (make_mld6v2_msg(type, code, src, dst, group, index, delay, 
			datalen, alert, sflag, qrv, qqic, gss) == FALSE)
	return FALSE;

    dstp = (struct sockaddr_in6 *) sndmh.msg_name;

#ifdef __KAME__
    if (IN6_IS_ADDR_LINKLOCAL(&dstp->sin6_addr) || 
	IN6_IS_ADDR_MC_LINKLOCAL(&dstp->sin6_addr))
	dstp->sin6_scope_id = index;
#endif

    if (sendmsg(mld6_socket, &sndmh, 0) < 0) {
	if (errno == ENETDOWN)
	    check_vif_state();
	else
	    log_msg(log_level(IPPROTO_ICMPV6, type, 0), errno,
		"sendmsg to %s with src %s on %s",
		sa6_fmt(dstp), src ? sa6_fmt(src) : "(unspec)",
		ifindex2str(index));

	return FALSE;
    }

    IF_DEBUG(DEBUG_PKT)
	log_msg(LOG_DEBUG, 0, "SENT %s from %-15s to %s",
	    packet_kind(IPPROTO_ICMPV6, type, 0),
	    src ? sa6_fmt(src) : "unspec", sa6_fmt(dstp));
    return TRUE;
}


/*
 * given a number, an exp. size in bits and a mantisse size in bits, return
 * the coded number value according to the code described in 
 * draft-vida-mld-v2-08.txt 
 * used to compute the Maximum Response Code (exp=3bit, mant=12bit) 
 * and the Querier Query interval Code (exp=3bit, mant=4 bit)
 * format  : |1|...exp...|...mant...|
 * if the number isn't representable there is a difference between realnbr 
 * and nbr if the number is too big return the max code value with a warning 
 */
unsigned int
codafloat(unsigned int nbr, unsigned int *realnbr, unsigned int sizeexp,
	  unsigned int sizemant)
{
	unsigned int mask = 0x1;
	unsigned int max = 0x0;
	unsigned int exp = 1;	/*exp value */
	unsigned int tmax;	/*max code value */
	unsigned int mantmask = 1;	/*mantisse mask */
	unsigned int onebit = 1;
	unsigned int mant;
	u_int16_t code = 1;	/* code */
	int i;

	/* compute maximal exp value */
	for (i = 1; i < sizeexp; i++)
		exp = (exp << 1) | 1;

	/* maximum size of this number in bits (after decoding) */
	tmax = exp + 3 + sizemant + 1;

	/* minimum value of this number */
	code <<= sizeexp + sizemant;
	mask <<= tmax - 1;

	/* maximum value of this number + a mantisse masque */
	for (i = 0; i <= sizemant; i++)
		max = max | mask >> i;
	for (i = 0; i < sizemant; i++)
		mantmask = mantmask | (onebit << i);

	/* not in coded number, so just return the given number as it is */
	if (nbr < code) {
		code = *realnbr = nbr;
		return code;
	}

	/* overflowed, so just return the possible max value */
	if (nbr > max) {
		*realnbr = max;
		return codafloat(max, realnbr, sizeexp, sizemant);
	}

	/* calculate the float number */
	while (!(nbr & mask)) {
		mask >>= 1;
		tmax--;
	}
	exp = tmax - (sizemant + 1);
	mant = nbr >> exp;
	exp -= 3;

	/* build code */
	mant &= mantmask;
	code |= mant;
	code |= exp << sizemant;

	/* compute effective value (draft-vida-mld-v2-08.txt p.11) */
	onebit <<= sizemant;
	*realnbr = (mant | onebit) << (exp + 3);
	return code;
}

unsigned int
decodeafloat(unsigned int nbr,unsigned int sizeexp,unsigned int sizemant)
{
	unsigned int onebit = 1;
	unsigned int mantmask = 0;
	unsigned int mant = 0;
	unsigned int exp = 0;
	int i;
	
    	for (i = 0; i < sizemant; i++)
		mantmask = mantmask | (onebit << i);
	mant = nbr & mantmask;
	exp = (nbr & ~(onebit << (sizemant + sizeexp))) >> sizemant;
	onebit <<= sizemant;
	return (mant | onebit) << (exp + 3);
}
#endif
