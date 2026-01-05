/*	$KAME: route.h,v 1.6 2001/08/09 08:46:58 suz Exp $	*/

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


#ifndef ROUTE_H
#define ROUTE_H

#include <sys/queue.h>

#ifndef TAILQ_FIRST
#define TAILQ_FIRST(head)	((head)->tqh_first)
#endif
#ifndef TAILQ_EMPTY
#define TAILQ_EMPTY(head)	(!TAILQ_FIRST(head))
#endif
#ifndef TAILQ_NEXT
#define TAILQ_NEXT(elm,field)	((elm)->field.tqe_next)
#endif
#ifndef TAILQ_FOREACH
#define TAILQ_FOREACH(var, head, field)			\
	for ((var) = TAILQ_FIRST((head));		\
	     (var);					\
	     (var) = TAILQ_NEXT((var), field))
#endif

struct staticrt {
	struct sockaddr_in6 paddr;
	u_int8 plen;
	struct sockaddr_in6 gwaddr;
	TAILQ_ENTRY(staticrt) link;
};

extern u_int32 default_source_preference;
extern u_int32 default_source_metric;

int change_interfaces(	mrtentry_t *mrtentry_ptr,mifi_t new_iif,
						if_set *new_joined_oifs,if_set *new_pruned_oifs,if_set *new_leaves_ , if_set *asserted ,
						u_int16 flags);

extern void      process_kernel_call     (void);
extern int  set_incoming        (srcentry_t *srcentry_ptr,
                         int srctype);
extern mifi_t   get_iif         (struct sockaddr_in6 *source);
extern int  add_sg_oif      (mrtentry_t *mrtentry_ptr,
                         mifi_t vifi,
                         u_int16 holdtime,
                         int update_holdtime);
extern void add_leaf        (mifi_t vifi, struct sockaddr_in6 *source,
                         struct sockaddr_in6 *group);
extern void delete_leaf     (mifi_t vifi, struct sockaddr_in6 *source, 
                         struct sockaddr_in6 *group);




extern pim_nbr_entry_t *find_pim6_nbr (struct sockaddr_in6 *source);
extern void calc_oifs       (mrtentry_t *mrtentry_ptr,
                         if_set *oifs_ptr);
extern void process_kernel_call (void);
extern int  delete_vif_from_mrt (mifi_t vifi);
extern mrtentry_t *switch_shortest_path (struct sockaddr_in6 *source, struct sockaddr_in6 *group);

extern struct staticrt * find_static_rt_entry (struct sockaddr_in6 *p);
extern int add_static_rt_entry (struct sockaddr_in6 *p, int plen, struct sockaddr_in6 *gw);

#endif
