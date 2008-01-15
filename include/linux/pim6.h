#ifndef _NETINET_PIM6_H
#define _NETINET_PIM6_H

#include <netinet/ip6.h>

struct pim {
#ifdef __LITTLE_ENDIAN_BITFIELD
	__u8	pim_type:4,
		pim_ver:4;
#else
	__u8	pim_ver:4,
		pim_type:4;
#endif
	__u8	pim_rsv;
	__u16	pim_cksum;
};

#define PIM_MINLEN	8
#define PIM6_REG_MINLEN	(PIM_MINLEN + sizeof(struct ip6_hdr))

#define PIM_REGISTER		1
#define PIM_NULL_REGISTER	0x40000000

#endif

