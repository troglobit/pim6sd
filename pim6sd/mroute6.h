#ifndef __LINUX_MROUTE6_H
#define __LINUX_MROUTE6_H

#include <linux/sockios.h>

/*
 *	Based on the MROUTING 3.5 defines primarily to keep
 *	source compatibility with BSD.
 *
 *	See the pim6sd code for the original history.
 *
 *      Protocol Independent Multicast (PIM) data structures included
 *      Carlos Picoto (cap@di.fc.ul.pt)
 *
 */

#define MRT6_BASE	200
#define MRT6_INIT	(MRT6_BASE)	/* Activate the kernel mroute code 	*/
#define MRT6_DONE	(MRT6_BASE+1)	/* Shutdown the kernel mroute		*/
#define MRT6_ADD_MIF	(MRT6_BASE+2)	/* Add a virtual interface		*/
#define MRT6_DEL_MIF	(MRT6_BASE+3)	/* Delete a virtual interface		*/
#define MRT6_ADD_MFC	(MRT6_BASE+4)	/* Add a multicast forwarding entry	*/
#define MRT6_DEL_MFC	(MRT6_BASE+5)	/* Delete a multicast forwarding entry	*/
#define MRT6_VERSION	(MRT6_BASE+6)	/* Get the kernel multicast version	*/
#define MRT6_ASSERT	(MRT6_BASE+7)	/* Activate PIM assert mode		*/
#define MRT6_PIM	(MRT6_BASE+8)	/* enable PIM code	*/

#define SIOCGETVIFCNT	SIOCPROTOPRIVATE	/* IP protocol privates */
#define SIOCGETSGCNT	(SIOCPROTOPRIVATE+1)
#define SIOCGETRPF	(SIOCPROTOPRIVATE+2)

#define MAXMIFS		32	
typedef unsigned long mifbitmap_t;	/* User mode code depends on this lot */
typedef unsigned short mifi_t;
#define ALL_MIFS	((mifi_t)(-1))

#ifndef IF_SETSIZE
#define IF_SETSIZE      256
#endif

typedef u_int32_t       if_mask;
#define NIFBITS (sizeof(if_mask) * 8)        /* bits per mask */

#ifndef howmany
#define howmany(x, y)   (((x) + ((y) - 1)) / (y))
#endif

typedef struct if_set {
	        if_mask ifs_bits[howmany(IF_SETSIZE, NIFBITS)];
} if_set;

#define IF_SET(n, p)    ((p)->ifs_bits[(n)/NIFBITS] |= (1 << ((n) % NIFBITS)))
#define IF_CLR(n, p)    ((p)->ifs_bits[(n)/NIFBITS] &= ~(1 << ((n) % NIFBITS)))
#define IF_ISSET(n, p)  ((p)->ifs_bits[(n)/NIFBITS] & (1 << ((n) % NIFBITS)))
#define IF_COPY(f, t)   bcopy(f, t, sizeof(*(f)))
#define IF_ZERO(p)      bzero(p, sizeof(*(p)))

/*
 	Same idea as select
 
#define VIFM_SET(n,m)	((m)|=(1<<(n)))
#define VIFM_CLR(n,m)	((m)&=~(1<<(n)))
#define VIFM_ISSET(n,m)	((m)&(1<<(n)))
#define VIFM_CLRALL(m)	((m)=0)
#define VIFM_COPY(mfrom,mto)	((mto)=(mfrom))
#define VIFM_SAME(m1,m2)	((m1)==(m2))
*/

/*
 *	Passed by mrouted for an MRT_ADD_MIF - again we use the
 *	mrouted 3.6 structures for compatibility
 */
 
struct mif6ctl {
	mifi_t	mif6c_mifi;		/* Index of MIF */
	unsigned char mif6c_flags;	/* MIFF_ flags */
	unsigned char vifc_threshold;	/* ttl limit */
	unsigned int vifc_rate_limit;	/* Rate limiter values (NI) */
	u_short	 mif6c_pifi;		/* the index of the physical IF */
};

#define MIFF_REGISTER	0x1	/* register vif	*/

/*
 *	Cache manipulation structures for mrouted and PIMd
 */
 
struct mf6cctl
{
	struct sockaddr_in6 mf6cc_origin;		/* Origin of mcast	*/
	struct sockaddr_in6 mf6cc_mcastgrp;		/* Group in question	*/
	mifi_t	mf6cc_parent;			/* Where it arrived	*/
	struct if_set mf6cc_ifset;		/* Where it is going */
	unsigned int mfcc_pkt_cnt;		/* pkt count for src-grp */
	unsigned int mfcc_byte_cnt;
	unsigned int mfcc_wrong_if;
	int	     mfcc_expire;
};

/* 
 *	Group count retrieval for mrouted
 */
 
struct sioc_sg_req6
{
	struct in6_addr src;
	struct in6_addr grp;
	unsigned long pktcnt;
	unsigned long bytecnt;
	unsigned long wrong_if;
};

/*
 *	To get vif packet counts
 */

struct sioc_mif_req6
{
	mifi_t	vifi;		/* Which iface */
	unsigned long icount;	/* In packets */
	unsigned long ocount;	/* Out packets */
	unsigned long ibytes;	/* In bytes */
	unsigned long obytes;	/* Out bytes */
};

/*
 *	That's all usermode folks
 */

#ifdef __KERNEL__
#include <net/sock.h>

extern int ip6_mroute_setsockopt(struct sock *, int, char __user *, int);
extern int ip6_mroute_getsockopt(struct sock *, int, char __user *, int __user *);
extern int ip6mr_ioctl(struct sock *sk, int cmd, void __user *arg);
extern void ip6_mr_init(void);


struct mif_device
{
	struct net_device 	*dev;			/* Device we are using */
	unsigned long	bytes_in,bytes_out;
	unsigned long	pkt_in,pkt_out;		/* Statistics 			*/
	unsigned long	rate_limit;		/* Traffic shaping (NI) 	*/
	unsigned char	threshold;		/* TTL threshold 		*/
	unsigned short	flags;			/* Control flags 		*/
	int		link;			/* Physical interface index	*/
};

#define VIFF_STATIC 0x8000

struct mfc6_cache 
{
	struct mfc6_cache *next;		/* Next entry on cache line 	*/
	struct in6_addr mf6c_mcastgrp;			/* Group the entry belongs to 	*/
	struct in6_addr mf6c_origin;			/* Source of packet 		*/
	mifi_t mf6c_parent;			/* Source interface		*/
	int mfc_flags;				/* Flags on line		*/

	union {
		struct {
			unsigned long expires;
			struct sk_buff_head unresolved;	/* Unresolved buffers		*/
		} unres;
		struct {
			unsigned long last_assert;
			int minvif;
			int maxvif;
			unsigned long bytes;
			unsigned long pkt;
			unsigned long wrong_if;
			unsigned char ttls[MAXMIFS];	/* TTL thresholds		*/
		} res;
	} mfc_un;
};

#define MFC_STATIC		1
#define MFC_NOTIFY		2

#define MFC6_LINES		64

#if  (MFC6_LINES & (MFC6_LINES -1 )) == 0
#define MF6CHASHMOD(h) ((h) & (MFC6_LINES -1))
#else
#define MF6CHASHMOD(h) ((h) % MFC6_LINES)
#endif

#define MFC6_HASH(a, g) MF6CHASHMOD((a).s6_addr32[0] ^ (a).s6_addr32[1] ^ \
				    (a).s6_addr32[2] ^ (a).s6_addr32[3] ^ \
				    (a).s6_addr32[0] ^ (a).s6_addr32[1] ^ \
				    (a).s6_addr32[2] ^ (a).s6_addr32[3])

#endif



#define MFC_ASSERT_THRESH (3*HZ)		/* Maximal freq. of asserts */

/*
 *	Pseudo messages used by mrouted
 */

#define IGMPMSG_NOCACHE		1		/* Kern cache fill request to mrouted */
#define IGMPMSG_WRONGVIF	2		/* For PIM assert processing (unused) */
#define IGMPMSG_WHOLEPKT	3		/* For PIM Register processing */

#ifdef __KERNEL__

#define PIM_V1_VERSION		__constant_htonl(0x10000000)
#define PIM_V1_REGISTER		1

#define PIM_VERSION		2
#define PIM_REGISTER		1

#define PIM_NULL_REGISTER	__constant_htonl(0x40000000)

/* PIMv2 register message header layout (ietf-draft-idmr-pimvsm-v2-00.ps */

struct pim6reghdr
{
	__u8	type;
	__u8	reserved;
	__u16	csum;
	__u32	flags;
};


struct rtmsg;
extern int ip6mr_get_route(struct sk_buff *skb, struct rtmsg *rtm, int nowait);
#endif

#ifdef __KERNEL__
#define IN6_ARE_ADDR_EQUAL(a,b) \
	(memcmp(&(a)->s6_addr[0], &(b)->s6_addr[0], sizeof(struct in6_addr)) == 0)
#endif

/*
 * Structure used to communicate from kernel to multicast router.
 * We'll overlay the structure onto an MLD header (not an IPv6 heder like igmpmsg{}
 * used for IPv4 implementation). This is because this structure will be passed via an
 * IPv6 raw socket, on wich an application will only receiver the payload i.e the data after
 * the IPv6 header and all the extension headers. (See section 3 of RFC 3542)
 */

struct mrt6msg {
#define MRT6MSG_NOCACHE		1
#define MRT6MSG_WRONGMIF	2
#define MRT6MSG_WHOLEPKT	3		/* used for use level encap */
	u_char		im6_mbz;		/* must be zero		   */
	u_char		im6_msgtype;		/* what type of message    */
	u_int16_t	im6_mif;		/* mif rec'd on		   */
	u_int32_t	im6_pad;		/* padding for 64 bit arch */
	struct in6_addr	im6_src, im6_dst;
};

/* XXX :there should not be there  */
#include <linux/icmpv6.h>

struct mld_hdr {
	struct icmp6hdr mld_icmp6_hdr;
	struct in6_addr	mld_addr;
};

#define mld_type mld_icmp6_hdr.icmp6_type


#endif
