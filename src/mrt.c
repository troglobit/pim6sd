/*	$KAME: mrt.c,v 1.19 2003/09/04 08:02:06 suz Exp $	*/

/*
 * Copyright (c) 1998-2001
 * The University of Southern California/Information Sciences Institute.
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

#include "defs.h"
#include "mrt.h"
#include "vif.h"
#include "rp.h"
#include "pim6.h"
#include "pimd.h"
#include "debug.h"
#include "mld6.h"
#include "inet6.h"
#include "timer.h"
#include "route.h"
#include "kern.h"

srcentry_t     *srclist;
grpentry_t     *grplist;

/*
 * Local functions definition
 */
static srcentry_t *create_srcentry 	(struct sockaddr_in6 *source);
static int search_srclist 			(struct sockaddr_in6 *source ,	
     								srcentry_t ** sourceEntry);

static int search_srcmrtlink 		(srcentry_t * srcentry_ptr,
				                  	struct sockaddr_in6 *group,
				                    mrtentry_t ** mrtPtr);

static void insert_srcmrtlink 		(mrtentry_t * elementPtr,
				                    mrtentry_t * insertPtr,
				                  	srcentry_t * srcListPtr);

static grpentry_t *create_grpentry  (struct sockaddr_in6 *group);

static int search_grplist 			(struct sockaddr_in6 *group,
				                 		grpentry_t ** groupEntry);

static int search_grpmrtlink 		(grpentry_t * grpentry_ptr,
				                      	 struct sockaddr_in6 *source,
				                      	 mrtentry_t ** mrtPtr);

static void insert_grpmrtlink 		(mrtentry_t * elementPtr,
				                     	 mrtentry_t * insertPtr,
				                  		 grpentry_t * grpListPtr);

static mrtentry_t *alloc_mrtentry 	(srcentry_t * srcentry_ptr,
				                		grpentry_t * grpentry_ptr);

static mrtentry_t *create_mrtentry 	(srcentry_t * srcentry_ptr,
				                  		grpentry_t * grpentry_ptr,
					                    u_int16 flags);

static void move_kernel_cache 		(mrtentry_t * mrtentry_ptr,
				                        u_int16 flags);

void
init_pim6_mrt()
{

    /* TODO: delete any existing routing table */

    /* Initialize the source list */
    /* The first entry has address 'IN6ADDR_ANY' and is not used */
    /* The order is the smallest address first. */

    srclist = malloc(sizeof(*srclist));
    if (!srclist)
	    log_msg(LOG_ERR, 0, "ran out of memory");	/* fatal */

    srclist->next = NULL;
    srclist->prev = NULL;
    init_sin6(&srclist->address);
    srclist->mrtlink = NULL;
    srclist->incoming = NO_VIF;
    srclist->upstream = NULL;
    srclist->metric = 0;
    srclist->preference = 0;
    RESET_TIMER(srclist->timer);
    srclist->cand_rp = NULL;

    /* Initialize the group list */
    /* The first entry has address 'IN6ADDR_ANY' and is not used */
    /* The order is the smallest address first. */

    grplist = malloc(sizeof(*grplist));
    if (!grplist)
	    log_msg(LOG_ERR, 0, "ran out of memory");	/* fatal */

    grplist->next = NULL;
    grplist->prev = NULL;
    grplist->rpnext = NULL;
    grplist->rpprev = NULL;
    init_sin6(&grplist->group);
    init_sin6(&grplist->rpaddr);
    grplist->mrtlink = NULL;
    grplist->active_rp_grp = (rp_grp_entry_t *) NULL;
    grplist->grp_route = NULL;
}

void
free_pim6_mrt()
{
    free(srclist);
    free(grplist);
}

grpentry_t     *
find_group(group)
    struct sockaddr_in6		*group;
{
    grpentry_t     *grpentry_ptr;

    if (!IN6_IS_ADDR_MULTICAST(&group->sin6_addr))
	return NULL;

    if (search_grplist(group, &grpentry_ptr) == TRUE)
	return (grpentry_ptr);	/* Group found! */

    return NULL;
}


srcentry_t     *
find_source(source)
    struct sockaddr_in6		*source;
{
    srcentry_t     *srcentry_ptr;

    if (!inet6_valid_host(source))
	return NULL;

    if (search_srclist(source, &srcentry_ptr) == TRUE)
	return (srcentry_ptr);	/* Source found! */

    return NULL;
}


mrtentry_t     *
find_route(source, group, flags, create)
    struct sockaddr_in6		*source,
                    		*group;
    u_int16         		flags;
    char            		create;
{
    srcentry_t     *srcentry_ptr;
    grpentry_t     *grpentry_ptr;
    mrtentry_t     *mrtentry_ptr;
    mrtentry_t     *mrtentry_ptr_wc;
    mrtentry_t     *mrtentry_ptr_pmbr;
    mrtentry_t     *mrtentry_ptr_2;
    rpentry_t      *rpentry_ptr=NULL;
    rp_grp_entry_t *rp_grp_entry_ptr;

    if (flags & (MRTF_SG | MRTF_WC))
    {
	if (!IN6_IS_ADDR_MULTICAST(&group->sin6_addr))
	    return NULL;
    }

    if (flags & MRTF_SG)
	if (!inet6_valid_host(source))
	    return NULL;

    if (create == DONT_CREATE)
    {
	if (flags & (MRTF_SG | MRTF_WC))
	{
	    if (search_grplist(group, &grpentry_ptr) == FALSE)
	    {
		/* Group not found. Return the (*,*,RP) entry */
		if (flags & MRTF_PMBR)
		{
		    rpentry_ptr = rp_match(group);
		    if (rpentry_ptr != NULL)
			return (rpentry_ptr->mrtlink);
		}
		return NULL;
	    }
	    /* Search for the source */
	    if (flags & MRTF_SG)
	    {
		if (search_grpmrtlink(grpentry_ptr, source,
				      &mrtentry_ptr) == TRUE)
		{
		    /* Exact (S,G) entry found */
		    return (mrtentry_ptr);
		}
	    }
	    /* No (S,G) entry. Return the (*,G) entry (if exist) */
	    if ((flags & MRTF_WC) &&
		(grpentry_ptr->grp_route != NULL))
		return (grpentry_ptr->grp_route);
	}

	/* Return the (*,*,RP) entry */

	if (flags & MRTF_PMBR)
	{
	    rpentry_ptr = NULL;
	    if (group != NULL)
		rpentry_ptr = rp_match(group);
	    else if (source != NULL)
		rpentry_ptr = rp_find(source);

	    if (rpentry_ptr != NULL)
		return (rpentry_ptr->mrtlink);
	}
	return NULL;
    }


    /* Creation allowed */
    if (flags & (MRTF_RP | MRTF_WC | MRTF_SG))
    {

	grpentry_ptr = create_grpentry(group);
	if (grpentry_ptr == NULL)
	    return NULL;
    }

    /*
     * Try to find an RP for mrt using RP.
     * First hop (S,G) has to refer to the corresponding RP 
     * unless G is in SSM range.  However SSM range check is skipped 
     * here, since this function is never called if G is in SSM range
     * (cache miss is impossible in SSM world).
     * Draft-ietf-pim-sm-v2-new-06.txt does not mention explicitly
     * whether first-hop DR has to create an (S,G) entry without RP(G) 
     * or not.  However RP(G) existence is checked since PIM register
     * packet cannot be advertised without RP(G) info.
     */
    if (flags & (MRTF_RP | MRTF_WC | MRTF_1ST))
    {
	if (grpentry_ptr->active_rp_grp == (rp_grp_entry_t *) NULL)
	{
	    rp_grp_entry_ptr = rp_grp_match(group);

	    if (rp_grp_entry_ptr == (rp_grp_entry_t *) NULL)
	    {
		if ((grpentry_ptr->mrtlink == NULL)
		    && (grpentry_ptr->grp_route == NULL))
		{
		    /* New created grpentry. Delete it. */
		    delete_grpentry(grpentry_ptr);
		}

		return NULL;
	    }

	    rpentry_ptr = rp_grp_entry_ptr->rp->rpentry;
	    grpentry_ptr->active_rp_grp = rp_grp_entry_ptr;
	    grpentry_ptr->rpaddr = rpentry_ptr->address;

	    /* Link to the top of the rp_grp_chain */

	    grpentry_ptr->rpnext = rp_grp_entry_ptr->grplink;
	    rp_grp_entry_ptr->grplink = grpentry_ptr;
	    if (grpentry_ptr->rpnext != NULL)
		grpentry_ptr->rpnext->rpprev = grpentry_ptr;
	}
	else
	    rpentry_ptr = grpentry_ptr->active_rp_grp->rp->rpentry;

	/* 
	 * don't accept the RP without any PIM neighbor 
	 * (except when RP is myself)
	 */
	if (rpentry_ptr->upstream == NULL &&
	    rpentry_ptr->incoming != reg_vif_num) {
		delete_grpentry(grpentry_ptr);
		return NULL;
	}
    }

    mrtentry_ptr_wc = mrtentry_ptr_pmbr = NULL;

    if (flags & MRTF_WC)
    {
	/* Setup the (*,G) routing entry */
	mrtentry_ptr_wc = create_mrtentry(NULL, grpentry_ptr, MRTF_WC);
	if (mrtentry_ptr_wc == NULL)
	{
	    if (grpentry_ptr->mrtlink == NULL)
	    {
		/* New created grpentry. Delete it. */
		delete_grpentry(grpentry_ptr);
	    }

	    return NULL;
	}

	if (mrtentry_ptr_wc->flags & MRTF_NEW)
	{
	    mrtentry_ptr_pmbr = rpentry_ptr->mrtlink;

	    /* Copy the oif list from the (*,*,RP) entry */
	    if (mrtentry_ptr_pmbr != NULL)
	    {
		VOIF_COPY(mrtentry_ptr_pmbr, mrtentry_ptr_wc);
	    }

	    mrtentry_ptr_wc->incoming = rpentry_ptr->incoming;
	    mrtentry_ptr_wc->upstream = rpentry_ptr->upstream;
	    mrtentry_ptr_wc->metric = rpentry_ptr->metric;
	    mrtentry_ptr_wc->preference = rpentry_ptr->preference;
	    move_kernel_cache(mrtentry_ptr_wc, 0);

#ifdef RSRR
	    rsrr_cache_bring_up(mrtentry_ptr_wc);
#endif				/* RSRR */

	}

	if (!(flags & MRTF_SG))
	    return (mrtentry_ptr_wc);
    }

    if (flags & MRTF_SG)
    {
	/* Setup the (S,G) routing entry */
	srcentry_ptr = create_srcentry(source);
	if (srcentry_ptr == NULL)
	{
	    /* TODO: XXX: The MRTF_NEW flag check may be misleading?? check */

	    if (((grpentry_ptr->grp_route == NULL)
		 || ((grpentry_ptr->grp_route != NULL)
		     && (grpentry_ptr->grp_route->flags & MRTF_NEW)))
		&& (grpentry_ptr->mrtlink == NULL))
	    {
		/* New created grpentry. Delete it. */
		delete_grpentry(grpentry_ptr);
	    }

	    return NULL;
	}

	mrtentry_ptr = create_mrtentry(srcentry_ptr, grpentry_ptr, MRTF_SG);
	if (mrtentry_ptr == NULL)
	{
	    if (((grpentry_ptr->grp_route == NULL)
		 || ((grpentry_ptr->grp_route != NULL)
		     && (grpentry_ptr->grp_route->flags & MRTF_NEW)))
		&& (grpentry_ptr->mrtlink == NULL))
	    {
		/* New created grpentry. Delete it. */
		delete_grpentry(grpentry_ptr);
	    }
	    if (srcentry_ptr->mrtlink == NULL)
	    {
		/* New created srcentry. Delete it. */
		delete_srcentry(srcentry_ptr);
	    }

	    return NULL;
	}

	if (mrtentry_ptr->flags & MRTF_NEW)
	{
	    /* 
	     * Copy the oif list from (*,G) or (*,*,RP) entry if it
	     * exists and G is an non-SSM prefix
	     */

	    if (SSMGROUP(group))
		goto not_copy;

	    mrtentry_ptr_2 = grpentry_ptr->grp_route;
	    if (mrtentry_ptr_2 == NULL) {
		if (grpentry_ptr->active_rp_grp != NULL) {
		    rpentry_ptr = grpentry_ptr->active_rp_grp->rp->rpentry;
		    mrtentry_ptr_2 = rpentry_ptr->mrtlink;
		    goto found_mrtentry_ptr_2;
		}
		rp_grp_entry_ptr = rp_grp_match(group);
		if (rp_grp_entry_ptr == NULL) {
		    mrtentry_ptr_2 = NULL;
		    goto found_mrtentry_ptr_2;
		}

		rpentry_ptr = rp_grp_entry_ptr->rp->rpentry;
		mrtentry_ptr_2 = rpentry_ptr->mrtlink;

		grpentry_ptr->active_rp_grp = rp_grp_entry_ptr;
		grpentry_ptr->rpaddr = rpentry_ptr->address;

		/* Link to the top of the rp_grp_chain */
	    	grpentry_ptr->rpnext = rp_grp_entry_ptr->grplink;
		rp_grp_entry_ptr->grplink = grpentry_ptr;
		if (grpentry_ptr->rpnext != NULL)
		    grpentry_ptr->rpnext->rpprev = grpentry_ptr;
	    }

	found_mrtentry_ptr_2:
	    if (mrtentry_ptr_2 != NULL) {
		VOIF_COPY(mrtentry_ptr_2, mrtentry_ptr);
		if (flags & MRTF_RP) {
		    /* ~(S,G) prune entry */
		    mrtentry_ptr->incoming = mrtentry_ptr_2->incoming;
		    mrtentry_ptr->upstream = mrtentry_ptr_2->upstream;
		    mrtentry_ptr->metric = mrtentry_ptr_2->metric;
		    mrtentry_ptr->preference = mrtentry_ptr_2->preference;
		    mrtentry_ptr->flags |= MRTF_RP;
		}
	    }

    not_copy:
	    if (!(mrtentry_ptr->flags & MRTF_RP)) {
		mrtentry_ptr->incoming = srcentry_ptr->incoming;
		mrtentry_ptr->upstream = srcentry_ptr->upstream;
		mrtentry_ptr->metric = srcentry_ptr->metric;
		mrtentry_ptr->preference = srcentry_ptr->preference;
	    }
	    move_kernel_cache(mrtentry_ptr, 0);

	    /*
	     * install (S,G) entry when there's no corresponding kernel cache
	     * for (*,G) nor (*,*,RP), if it's non-RPT route.
	     */
	    if ((mrtentry_ptr->flags & (MRTF_RP | MRTF_KERNEL_CACHE)) == 0)
		add_kernel_cache(mrtentry_ptr, source, group, 0);
#ifdef RSRR
	    rsrr_cache_bring_up(mrtentry_ptr);
#endif				/* RSRR */
	}
	return (mrtentry_ptr);
    }

    if (flags & MRTF_PMBR)
    {
	/* Get/return the (*,*,RP) routing entry */

	if (group != NULL) {
	    rpentry_ptr = rp_match(group);
	    if (rpentry_ptr == NULL)
		return NULL;
	} else
	    if (source != NULL)
	    {
		rpentry_ptr = rp_find(source);
		if (rpentry_ptr == NULL)
		    return NULL;
	    }
	    else
		return NULL;	/* source == group == IN6ADDR_ANY */

	if (rpentry_ptr->mrtlink != NULL)
	    return (rpentry_ptr->mrtlink);

	mrtentry_ptr = create_mrtentry(rpentry_ptr, NULL, MRTF_PMBR);
	if (mrtentry_ptr == NULL)
	    return NULL;

	mrtentry_ptr->incoming = rpentry_ptr->incoming;
	mrtentry_ptr->upstream = rpentry_ptr->upstream;
	mrtentry_ptr->metric = rpentry_ptr->metric;
	mrtentry_ptr->preference = rpentry_ptr->preference;
	return (mrtentry_ptr);
    }

    return NULL;
}


void
delete_srcentry(srcentry_ptr)
    srcentry_t     *srcentry_ptr;
{
    mrtentry_t     *mrtentry_ptr;
    mrtentry_t     *mrtentry_next;

    if (srcentry_ptr == NULL)
	return;

    /* TODO: XXX: the first entry is unused and always there */

    srcentry_ptr->prev->next = srcentry_ptr->next;
    if (srcentry_ptr->next != NULL)
	srcentry_ptr->next->prev = srcentry_ptr->prev;

    for (mrtentry_ptr = srcentry_ptr->mrtlink;
	 mrtentry_ptr != NULL;
	 mrtentry_ptr = mrtentry_next)
    {
	mrtentry_next = mrtentry_ptr->srcnext;
	if (mrtentry_ptr->flags & MRTF_KERNEL_CACHE)
	    /* Delete the kernel cache first */
	    delete_mrtentry_all_kernel_cache(mrtentry_ptr);

	if (mrtentry_ptr->grpprev != NULL)
	    mrtentry_ptr->grpprev->grpnext = mrtentry_ptr->grpnext;
	else
	{
	    mrtentry_ptr->group->mrtlink = mrtentry_ptr->grpnext;
	    if ((mrtentry_ptr->grpnext == NULL)
		&& (mrtentry_ptr->group->grp_route == NULL))
	    {
		/* Delete the group entry if it has no (*,G) routing entry */
		delete_grpentry(mrtentry_ptr->group);
	    }
	}

	if (mrtentry_ptr->grpnext != NULL)
	    mrtentry_ptr->grpnext->grpprev = mrtentry_ptr->grpprev;
	FREE_MRTENTRY(mrtentry_ptr);
    }
    free(srcentry_ptr);
}


void
delete_grpentry(grp)
    grpentry_t     *grp;
{
    mrtentry_t     *node;
    mrtentry_t     *next;

    if (grp == NULL)
	return;

    /* TODO: XXX: the first entry is unused and always there */

    grp->prev->next = grp->next;
    if (grp->next != NULL)
	grp->next->prev = grp->prev;

    if (grp->grp_route != NULL)
    {
	if (grp->grp_route->flags & MRTF_KERNEL_CACHE)
	    delete_mrtentry_all_kernel_cache(grp->grp_route);
	FREE_MRTENTRY(grp->grp_route);
    }

    /* Delete from the rp_grp_entry chain */
    if (grp->active_rp_grp != (rp_grp_entry_t *) NULL)
    {
	if (grp->rpnext != NULL)
	    grp->rpnext->rpprev = grp->rpprev;

	if (grp->rpprev != NULL)
	    grp->rpprev->rpnext = grp->rpnext;
	else
	    grp->active_rp_grp->grplink = grp->rpnext;

	/* if necessary, delete the RP entry calculated by embedded-RP */
	if (grp->active_rp_grp != NULL &&
	    grp->active_rp_grp->grplink == NULL &&
	    grp->active_rp_grp->origin == RP_ORIGIN_EMBEDDEDRP)
	    delete_rp_grp_entry(&cand_rp_list, &grp_mask_list, grp->active_rp_grp);
    }

    for (node = grp->mrtlink; node != NULL; node = next)
    {
	next = node->grpnext;
	if (node->flags & MRTF_KERNEL_CACHE)
	    /* Delete the kernel cache first */
	    delete_mrtentry_all_kernel_cache(node);

	if (node->srcprev != NULL)
	    node->srcprev->srcnext = node->srcnext;
	else
	{
	    node->source->mrtlink = node->srcnext;
	    if (node->srcnext == NULL)
	    {
		/* Delete the srcentry if this was the last routing entry */
		delete_srcentry(node->source);
	    }
	}

	if (node->srcnext != NULL)
	    node->srcnext->srcprev = node->srcprev;
	FREE_MRTENTRY(node);
    }
    free(grp);
}


void
delete_mrtentry(mrt)
    mrtentry_t     *mrt;
{
    grpentry_t     *grp;
    mrtentry_t     *mrt_wc;
    mrtentry_t     *mrt_rp;

    if (mrt == NULL)
	return;

    /* Delete the kernel cache first */
    if (mrt->flags & MRTF_KERNEL_CACHE)
	delete_mrtentry_all_kernel_cache(mrt);

#ifdef RSRR
    /* Tell the reservation daemon */
    rsrr_cache_clean(mrt);
#endif				/* RSRR */

    if (mrt->flags & MRTF_PMBR)
    {
	/* (*,*,RP) mrtentry */
	mrt->source->mrtlink = NULL;
    }
    else
    {
	if (mrt->flags & MRTF_SG)
	{
	    /* (S,G) mrtentry */
	    /* Delete from the grpentry MRT chain */
	    if (mrt->grpprev != NULL)
		mrt->grpprev->grpnext = mrt->grpnext;
	    else
	    {
		mrt->group->mrtlink = mrt->grpnext;
		if (mrt->grpnext == NULL)
		{
		    /*
		     * All (S,G) MRT entries are gone. Allow creating (*,G)
		     * MFC entries.
		     */
		    if (!SSMGROUP(&mrt->group->group))
			mrt_rp = mrt->group->active_rp_grp->rp->rpentry->mrtlink;
		    else
			mrt_rp = NULL;

		    mrt_wc = mrt->group->grp_route;
		    if (mrt_rp != NULL)
			mrt_rp->flags &= ~MRTF_MFC_CLONE_SG;
		    if (mrt_wc != NULL)
			mrt_wc->flags &= ~MRTF_MFC_CLONE_SG;
		    else
		    {
			/*
			 * Delete the group entry if it has no (*,G) routing
			 * entry
			 */
			delete_grpentry(mrt->group);
		    }
		}
	    }

	    if (mrt->grpnext != NULL)
		mrt->grpnext->grpprev = mrt->grpprev;

	    /* Delete from the srcentry MRT chain */
	    if (mrt->srcprev != NULL)
		mrt->srcprev->srcnext = mrt->srcnext;
	    else
	    {
		mrt->source->mrtlink = mrt->srcnext;
		if (mrt->srcnext == NULL)
		{
		    /* Delete the srcentry if this was the last routing entry */
		    delete_srcentry(mrt->source);
		}
	    }

	    if (mrt->srcnext != NULL)
		mrt->srcnext->srcprev = mrt->srcprev;
	}
	else
	{
	    /* This mrtentry should be (*,G) */
	    grp = mrt->group;
	    grp->grp_route = NULL;

	    if (grp->mrtlink == NULL)
		/* Delete the group entry if it has no (S,G) entries */
		delete_grpentry(grp);
	}
    }

    FREE_MRTENTRY(mrt);
}


static int
search_srclist(source, sourceEntry)
    struct sockaddr_in6         *source;
    srcentry_t 	**sourceEntry;
{
    srcentry_t 	*s_prev, *s;

    for (s_prev = srclist, s = s_prev->next; s != NULL;
	 s_prev = s, s = s->next)
    {
	/*
	 * The srclist is ordered with the smallest addresses first. The
	 * first entry is not used.
	 */
	if (inet6_lessthan(&s->address, source))
	    continue;

	if (inet6_equal(&s->address, source))
	{
	    *sourceEntry = s;
	    return (TRUE);
	}
	break;
    }
    *sourceEntry = s_prev;	/* The insertion point is between s_prev and
				 * s */
    return (FALSE);
}


static int
search_grplist(group, groupEntry)
    struct sockaddr_in6      	*group;
    grpentry_t 	**groupEntry;
{
    grpentry_t *g_prev, *g;

    for (g_prev = grplist, g = g_prev->next; g != NULL;
	 g_prev = g, g = g->next)
    {
	/*
	 * The grplist is ordered with the smallest address first. The first
	 * entry is not used.
	 */
	if (inet6_lessthan(&g->group, group))
	    continue;

	if (inet6_equal(&g->group, group))
	{
	    *groupEntry = g;
	    return (TRUE);
	}
	break;
    }
    *groupEntry = g_prev;	/* The insertion point is between g_prev and
				 			* g */
    return (FALSE);
}


static srcentry_t *
create_srcentry(source)
    struct sockaddr_in6		*source;
{
    srcentry_t *node;
    srcentry_t *prev;

    if (search_srclist(source, &prev) == TRUE)
	return (prev);

    node = malloc(sizeof(*node));
    if (!node)
    {
	log_msg(LOG_WARNING, 0, "Memory allocation error for srcentry %s",
		sa6_fmt(source));
	return NULL;
    }

    node->address = *source;
    /*
     * Free the memory if there is error getting the iif and the next hop
     * (upstream) router.
     */
  
    if (set_incoming(node, PIM_IIF_SOURCE) == FALSE)
    {
	free(node);
	return NULL;
    }

    node->mrtlink = NULL;
    RESET_TIMER(node->timer);
    node->cand_rp = NULL;
    node->next = prev->next;
    prev->next = node;
    node->prev = prev;
    if (node->next != NULL)
	node->next->prev = node;

    IF_DEBUG(DEBUG_MFC)
	log_msg(LOG_DEBUG, 0, "create source entry, source %s", sa6_fmt(source));

    return (node);
}


static grpentry_t *
create_grpentry(group)
    struct sockaddr_in6		*group;
{
    grpentry_t *node;
    grpentry_t *prev;

    if (search_grplist(group, &prev) == TRUE)
	return (prev);

    node = malloc(sizeof(*node));
    if (!node)
    {
	log_msg(LOG_WARNING, 0, "Memory allocation error for grpentry %s", sa6_fmt(group));
	return NULL;
    }

    /*
     * TODO: XXX: Note that this is NOT a (*,G) routing entry, but simply a
     * group entry, probably used to search the routing table (to find (S,G)
     * entries for example.) To become (*,G) routing entry, we must setup
     * node->grp_route
     */

    node->group = *group;
    init_sin6(&node->rpaddr);
    node->mrtlink = NULL;
    node->active_rp_grp = (rp_grp_entry_t *) NULL;
    node->grp_route = NULL;
    node->rpnext = NULL;
    node->rpprev = NULL;

    /* Now it is safe to include the new group entry */

    node->next = prev->next;
    prev->next = node;
    node->prev = prev;
    if (node->next != NULL)
	node->next->prev = node;

    IF_DEBUG(DEBUG_MFC)
	log_msg(LOG_DEBUG, 0, "create group entry, group %s", sa6_fmt(group));

    return (node);
}


/*
 * Return TRUE if the entry is found and then *found is set to point to that
 * entry. Otherwise return FALSE and *found points the previous entry
 * (or NULL if first in the chain.
 */
static int
search_srcmrtlink(src, group, found)
    srcentry_t     		*src;
    struct sockaddr_in6		*group;
    mrtentry_t    		**found;
{
    mrtentry_t *node;
    mrtentry_t *prev = NULL;

    for (node = src->mrtlink; node != NULL; prev = node, node = node->srcnext)
    {
	/*
	 * The entries are ordered with the smaller group address first. The
	 * addresses are in network order.
	 */
	
	if (inet6_lessthan(&node->group->group, group))
	    continue;

	if (inet6_equal(&node->group->group, group))
	{
	    *found = node;
	    return (TRUE);
	}
	break;
    }
    *found = prev;

    return (FALSE);
}


/*
 * Return TRUE if the entry is found and then *found is set to point to that
 * entry. Otherwise return FALSE and *found points the previous entry
 * (or NULL if first in the chain.
 */
static int
search_grpmrtlink(grp, source, found)
    grpentry_t     		*grp;
    struct sockaddr_in6		*source;
    mrtentry_t    		**found;
{
    mrtentry_t *node;
    mrtentry_t *prev = NULL;

    for (node = grp->mrtlink; node != NULL; prev = node, node = node->grpnext)
    {
	/*
	 * The entries are ordered with the smaller source address first. The
	 * addresses are in network order.
	 */
	if (inet6_lessthan(&node->source->address, source))
	    continue;

	if (inet6_equal(source, &node->source->address))
	{
	    *found = node;
	    return (TRUE);
	}
	break;
    }
    *found = prev;

    return (FALSE);
}


static void
insert_srcmrtlink(mrtentry_new, mrtentry_prev, srcentry_ptr)
    mrtentry_t     *mrtentry_new;
    mrtentry_t     *mrtentry_prev;
    srcentry_t     *srcentry_ptr;
{
    if (mrtentry_prev == NULL)
    {
	/* Has to be insert as the head entry for this source */
	mrtentry_new->srcnext = srcentry_ptr->mrtlink;
	mrtentry_new->srcprev = NULL;
	srcentry_ptr->mrtlink = mrtentry_new;
    }
    else
    {
	/* Insert right after the mrtentry_prev */
	mrtentry_new->srcnext = mrtentry_prev->srcnext;
	mrtentry_new->srcprev = mrtentry_prev;
	mrtentry_prev->srcnext = mrtentry_new;
    }

    if (mrtentry_new->srcnext != NULL)
	mrtentry_new->srcnext->srcprev = mrtentry_new;
}


static void
insert_grpmrtlink(mrtentry_new, mrtentry_prev, grpentry_ptr)
    mrtentry_t     *mrtentry_new;
    mrtentry_t     *mrtentry_prev;
    grpentry_t     *grpentry_ptr;
{
    if (mrtentry_prev == NULL)
    {
	/* Has to be insert as the head entry for this group */
	mrtentry_new->grpnext = grpentry_ptr->mrtlink;
	mrtentry_new->grpprev = NULL;
	grpentry_ptr->mrtlink = mrtentry_new;
    }
    else
    {
	/* Insert right after the mrtentry_prev */
	mrtentry_new->grpnext = mrtentry_prev->grpnext;
	mrtentry_new->grpprev = mrtentry_prev;
	mrtentry_prev->grpnext = mrtentry_new;
    }

    if (mrtentry_new->grpnext != NULL)
	mrtentry_new->grpnext->grpprev = mrtentry_new;
}


static mrtentry_t *
alloc_mrtentry(srcentry_ptr, grpentry_ptr)
    srcentry_t     *srcentry_ptr;
    grpentry_t     *grpentry_ptr;
{
    mrtentry_t *mrtentry_ptr;
    u_int16         	i,
                   	*i_ptr;
    u_int8          	vif_numbers;

    mrtentry_ptr = malloc(sizeof(*mrtentry_ptr));
    if (!mrtentry_ptr)
    {
	log_msg(LOG_WARNING, 0, "alloc_mrtentry(): out of memory");
	return NULL;
    }

    /*
     * grpnext, grpprev, srcnext, srcprev will be setup when we link the
     * mrtentry to the source and group chains
     */
    mrtentry_ptr->source = srcentry_ptr;
    mrtentry_ptr->group = grpentry_ptr;
    mrtentry_ptr->incoming = NO_VIF;
    IF_ZERO(&mrtentry_ptr->joined_oifs);
    IF_ZERO(&mrtentry_ptr->leaves);
    IF_ZERO(&mrtentry_ptr->pruned_oifs);
    IF_ZERO(&mrtentry_ptr->asserted_oifs);
    IF_ZERO(&mrtentry_ptr->oifs);
    mrtentry_ptr->upstream = NULL;
    mrtentry_ptr->metric = 0;
    mrtentry_ptr->preference = 0;
    init_sin6(&mrtentry_ptr->pmbr_addr);

#ifdef RSRR
    mrtentry_ptr->rsrr_cache = NULL;
#endif

    /*
     * XXX: TODO: if we are short in memory, we can reserve as few as
     * possible space for vif timers (per group and/or routing entry), but
     * then everytime when a new interfaces is configured, the router will be
     * restarted and will delete the whole routing table. The "memory is
     * cheap" solution is to reserve timer space for all potential vifs in
     * advance and then no need to delete the routing table and disturb the
     * forwarding.
     */

#ifdef SAVE_MEMORY
    mrtentry_ptr->vif_timers = malloc(sizeof(u_int16) * numvifs);
    mrtentry_ptr->vif_deletion_delay = malloc(sizeof(u_int16) * numvifs);
    vif_numbers = numvifs;
#else
    mrtentry_ptr->vif_timers = malloc(sizeof(u_int16) * total_interfaces);
    mrtentry_ptr->vif_deletion_delay = malloc(sizeof(u_int16) * total_interfaces);
    vif_numbers = total_interfaces;
#endif
    if (!mrtentry_ptr->vif_timers || !mrtentry_ptr->vif_deletion_delay)
    {
	log_msg(LOG_WARNING, 0, "alloc_mrtentry(): out of memory");
	FREE_MRTENTRY(mrtentry_ptr);
	return NULL;
    }

    /* Reset the timers */
    for (i = 0, i_ptr = mrtentry_ptr->vif_timers; i < vif_numbers;
	 i++, i_ptr++)
	RESET_TIMER(*i_ptr);
    for (i = 0, i_ptr = mrtentry_ptr->vif_deletion_delay; i < vif_numbers;
	 i++, i_ptr++)
	RESET_TIMER(*i_ptr);

    mrtentry_ptr->flags = MRTF_NEW;
    RESET_TIMER(mrtentry_ptr->timer);
    RESET_TIMER(mrtentry_ptr->jp_timer);
    RESET_TIMER(mrtentry_ptr->rs_timer);
    RESET_TIMER(mrtentry_ptr->assert_timer);
    RESET_TIMER(mrtentry_ptr->assert_rate_timer);
    mrtentry_ptr->kernel_cache = NULL;

    return (mrtentry_ptr);
}


static mrtentry_t *
create_mrtentry(srcentry_ptr, grpentry_ptr, flags)
    srcentry_t     		*srcentry_ptr;
    grpentry_t     		*grpentry_ptr;
    u_int16         		flags;
{
    mrtentry_t     		*r_new;
    mrtentry_t     		*r_grp_insert,
                   		*r_src_insert;	/* pointers to insert */
    struct sockaddr_in6		*source;
    struct sockaddr_in6		*group;

    if (flags & MRTF_SG)
    {
	/* (S,G) entry */
	source = &srcentry_ptr->address;
	group = &grpentry_ptr->group;

	if (search_grpmrtlink(grpentry_ptr, source, &r_grp_insert) == TRUE)
	    return (r_grp_insert);

	if (search_srcmrtlink(srcentry_ptr, group, &r_src_insert) == TRUE)
	{
	    /*
	     * Hmmm, search_grpmrtlink() didn't find the entry, but
	     * search_srcmrtlink() did find it! Shoudn't happen. Panic!
	     */

	    log_msg(LOG_ERR, 0, "MRT inconsistency for src %s and grp %s\n",
		sa6_fmt(source), sa6_fmt(group));
	    /* not reached but to make lint happy */
	    return NULL;
	}

	/*
	 * Create and insert in group mrtlink and source mrtlink chains.
	 */
	r_new = alloc_mrtentry(srcentry_ptr, grpentry_ptr);
	if (!r_new)
	    return NULL;

	/*
	 * r_new has to be insert right after r_grp_insert in the grp mrtlink
	 * chain and right after r_src_insert in the src mrtlink chain
	 */
	insert_grpmrtlink(r_new, r_grp_insert, grpentry_ptr);
	insert_srcmrtlink(r_new, r_src_insert, srcentry_ptr);
	r_new->flags |= MRTF_SG;

	IF_DEBUG(DEBUG_MFC)
	    log_msg(LOG_DEBUG, 0, "create SG entry, source %s, group %s",
		sa6_fmt(source), sa6_fmt(group));

	return (r_new);
    }

    if (flags & MRTF_WC)
    {
	/* (*,G) entry */
	if (grpentry_ptr->grp_route != NULL)
	    return (grpentry_ptr->grp_route);

	r_new = alloc_mrtentry(srcentry_ptr, grpentry_ptr);
	if (!r_new)
	    return NULL;

	grpentry_ptr->grp_route = r_new;
	r_new->flags |= (MRTF_WC | MRTF_RP);
	return (r_new);
    }

    if (flags & MRTF_PMBR)
    {
	/* (*,*,RP) entry */
	if (srcentry_ptr->mrtlink != NULL)
	    return (srcentry_ptr->mrtlink);

	r_new = alloc_mrtentry(srcentry_ptr, grpentry_ptr);
	if (!r_new)
	    return NULL;

	srcentry_ptr->mrtlink = r_new;
	r_new->flags |= (MRTF_PMBR | MRTF_RP);
	return (r_new);
    }

    return NULL;
}

/*
 * Delete all kernel cache for this mrtentry
 */
void
delete_mrtentry_all_kernel_cache(mrtentry_ptr)
    mrtentry_t     *mrtentry_ptr;
{
    kernel_cache_t *node, *prev;

    if (!(mrtentry_ptr->flags & MRTF_KERNEL_CACHE))
	return;

    /* Free all kernel_cache entries */
    node = mrtentry_ptr->kernel_cache;
    while (node)
    {
	prev = node;
	node = node->next;

	IF_DEBUG(DEBUG_MFC)
	    log_msg(LOG_DEBUG, 0, "Deleting MFC entry (all) for source %s and group %s",
		    sa6_fmt(&prev->source), sa6_fmt(&prev->group));

	k_del_mfc(mld6_socket, &prev->source, &prev->group);
	free(prev);
    }
    mrtentry_ptr->kernel_cache = NULL;

    /* turn off the cache flag(s) */
    mrtentry_ptr->flags &= ~(MRTF_KERNEL_CACHE | MRTF_MFC_CLONE_SG);
}


void
delete_single_kernel_cache(mrtentry_ptr, kernel_cache_ptr)
    mrtentry_t     *mrtentry_ptr;
    kernel_cache_t *kernel_cache_ptr;
{
    if (kernel_cache_ptr->prev == NULL)
    {
	mrtentry_ptr->kernel_cache = kernel_cache_ptr->next;
	if (mrtentry_ptr->kernel_cache == NULL)
	    mrtentry_ptr->flags &= ~(MRTF_KERNEL_CACHE | MRTF_MFC_CLONE_SG);
    }
    else
	kernel_cache_ptr->prev->next = kernel_cache_ptr->next;

    if (kernel_cache_ptr->next != NULL)
	kernel_cache_ptr->next->prev = kernel_cache_ptr->prev;

    IF_DEBUG(DEBUG_MFC)
	log_msg(LOG_DEBUG, 0, "Deleting MFC entry for source %s and group %s",
	    sa6_fmt(&kernel_cache_ptr->source),
	    sa6_fmt(&kernel_cache_ptr->group));
    k_del_mfc(mld6_socket, &kernel_cache_ptr->source, &kernel_cache_ptr->group);
    free(kernel_cache_ptr);
}


void
delete_single_kernel_cache_addr(mrtentry_ptr, source, group)
    mrtentry_t     	*mrtentry_ptr;
    struct sockaddr_in6 *source;
    struct sockaddr_in6 *group;
{
    kernel_cache_t *node;

    if (mrtentry_ptr == NULL)
	return;

    /* Find the exact (S,G) kernel_cache entry */
    for (node = mrtentry_ptr->kernel_cache;
	 node != NULL;
	 node = node->next)
    {
	if (inet6_lessthan(&node->group, group))
	    continue;
	if (inet6_greaterthan(&node->group, group))
	    return;		/* Not found */
	if (inet6_lessthan(&node->source, source))
	    continue;
	if (inet6_greaterthan(&node->source, source))
	    return;		/* Not found */
	/* Found exact match */
	break;
    }

    if (node == NULL)
	return;

    /* Found. Delete it */
    if (node->prev == NULL)
    {
	mrtentry_ptr->kernel_cache = node->next;
	if (mrtentry_ptr->kernel_cache == NULL)
	    mrtentry_ptr->flags &= ~(MRTF_KERNEL_CACHE | MRTF_MFC_CLONE_SG);
    }
    else
	node->prev->next = node->next;

    if (node->next != NULL)
	node->next->prev = node->prev;

    IF_DEBUG(DEBUG_MFC)
	log_msg(LOG_DEBUG, 0, "Deleting MFC entry for source %s and group %s",
	    sa6_fmt(&node->source),
	    sa6_fmt(&node->group));
    k_del_mfc(mld6_socket, &node->source, &node->group);
    free(node);
}


/*
 * Installs kernel cache for (source, group). Assumes mrtentry_ptr is the
 * correct entry.
 */
void
add_kernel_cache(mrtentry_ptr, source, group, flags)
    mrtentry_t     		*mrtentry_ptr;
    struct sockaddr_in6 	*source;
    struct sockaddr_in6 	*group;
    u_int16         		flags;
{
    kernel_cache_t *next;
    kernel_cache_t *prev;
    kernel_cache_t *node;

    if (mrtentry_ptr == NULL)
	return;

    move_kernel_cache(mrtentry_ptr, flags);

    if (mrtentry_ptr->flags & MRTF_SG)
    {
	/* (S,G) */
	if (mrtentry_ptr->flags & MRTF_KERNEL_CACHE)
	    return;

	node = malloc(sizeof(*node));
	if (!node)
		log_msg(LOG_ERR, 0, "ran out of memory");	/* fatal */

	node->next = NULL;
	node->prev = NULL;
	node->source = *source;
	node->group = *group;
	node->sg_count.pktcnt = 0;
	node->sg_count.bytecnt = 0;
	node->sg_count.wrong_if = 0;
	mrtentry_ptr->kernel_cache = node;
	mrtentry_ptr->flags |= MRTF_KERNEL_CACHE;
	return;
    }

    prev = NULL;

    for (next = mrtentry_ptr->kernel_cache;
	 next != NULL;
	 prev = next,
	 next = next->next)
    {
	if (inet6_lessthan(&next->group , group))
	    continue;
	if (inet6_greaterthan(&next->group , group))
	    break;
	if (inet6_lessthan(&next->source , source))
	    continue;
	if (inet6_greaterthan(&next->source , source))
	    break;
	/* Found exact match. Nothing to change. */
	return;
    }

    /*
     * The new entry must be placed between prev and
     * next
     */
    node = malloc(sizeof(*node));
    if (!node)
	    log_msg(LOG_ERR, 0, "ran out of memory");	/* fatal */

    if (prev)
	prev->next = node;
    else
	mrtentry_ptr->kernel_cache = node;
    if (next)
	next->prev = node;

    node->prev = prev;
    node->next = next;
    node->source = *source;
    node->group = *group;
    node->sg_count.pktcnt = 0;
    node->sg_count.bytecnt = 0;
    node->sg_count.wrong_if = 0;
    mrtentry_ptr->flags |= MRTF_KERNEL_CACHE;
}

/*
 * Bring the kernel cache "UP": from the (*,*,RP) to (*,G) or (S,G)
 */
static void
move_kernel_cache(mrtentry_ptr, flags)
    mrtentry_t     *mrtentry_ptr;
    u_int16         flags;
{
    kernel_cache_t *node;
    kernel_cache_t *insert;
    kernel_cache_t *first;
    kernel_cache_t *last;
    kernel_cache_t *prev;
    mrtentry_t     *mrtentry_pmbr;
    mrtentry_t     *mrtentry_rp;
    int             found;

    if (mrtentry_ptr == NULL)
	return;

    if (mrtentry_ptr->flags & MRTF_PMBR)
	return;

    if (mrtentry_ptr->flags & MRTF_WC)
    {
	/* Move the cache info from (*,*,RP) to (*,G) */
	mrtentry_pmbr = mrtentry_ptr->group->active_rp_grp->rp->rpentry->mrtlink;
	if (mrtentry_pmbr == NULL)
	    return;		/* Nothing to move */

	first = last = NULL;
	for (node = mrtentry_pmbr->kernel_cache; node != NULL; node = node->next)
	{
	    /*
	     * The order is: (1) smaller group; (2) smaller source within
	     * group
	     */
	    if (inet6_lessthan(&node->group, &mrtentry_ptr->group->group))
		continue;
	    if (!inet6_equal(&node->group, &mrtentry_ptr->group->group))
		break;

	    /* Select the kernel_cache entries to move  */
	    if (first == NULL)
		first = last = node;
	    else
		last = node;
	}

	if (first != NULL)
	{
	    /* Fix the old chain */
	    if (first->prev != NULL)
		first->prev->next = last->next;
	    else
		mrtentry_pmbr->kernel_cache = last->next;

	    if (last->next != NULL)
		last->next->prev = first->prev;
	    if (mrtentry_pmbr->kernel_cache == NULL)
		mrtentry_pmbr->flags &= ~(MRTF_KERNEL_CACHE | MRTF_MFC_CLONE_SG);

	    /* Insert in the new place */
	    prev = NULL;
	    last->next = NULL;
	    mrtentry_ptr->flags |= MRTF_KERNEL_CACHE;

	    for (node = mrtentry_ptr->kernel_cache; node != NULL;)
	    {
		if (first == NULL)
		    break;	/* All entries have been inserted */

		if (inet6_greaterthan(&node->source,&first->source))
		{
		    /* Insert the entry before node */
		    insert = first;
		    first = first->next;
		    if (node->prev != NULL)
			node->prev->next = insert;
		    else
			mrtentry_ptr->kernel_cache = insert;

		    insert->prev = node->prev;
		    insert->next = node;
		    node->prev   = insert;
		}

		prev = node;
		node = node->next;
	    }

	    if (first != NULL)
	    {
		/* Place all at the end after prev */
		if (prev != NULL)
		    prev->next = first;
		else
		    mrtentry_ptr->kernel_cache = first;
		first->prev = prev;
	    }
	}
	return;
    }

    if (mrtentry_ptr->flags & MRTF_SG)
    {
	/*
	 * (S,G) entry. Move the whole group cache from (*,*,RP) to (*,G) and
	 * then get the necessary entry from (*,G). TODO: Not optimized! The
	 * particular entry is moved first to (*,G), then we have to search
	 * again (*,G) to find it and move to (S,G).
	 */
	/* TODO: XXX: No need for this? Thinking.... */
	/* move_kernel_cache(mrtentry_ptr->group->grp_route, flags); */

	if (SSMGROUP(&mrtentry_ptr->group->group))
		return;

	mrtentry_rp = mrtentry_ptr->group->grp_route;
	if (mrtentry_rp == NULL)
	    mrtentry_rp = mrtentry_ptr->group->active_rp_grp->rp->rpentry->mrtlink;
	if (mrtentry_rp == NULL)
	    return;

	if (mrtentry_rp->incoming != mrtentry_ptr->incoming)
	{
	    /*
	     * XXX: the (*,*,RP) (or (*,G)) iif is different from the (S,G)
	     * iif. No need to move the cache, because (S,G) don't need it.
	     * After the first packet arrives on the shortest path, the
	     * correct cache entry will be created. If (flags &
	     * MFC_MOVE_FORCE) then we must move the cache. This usually
	     * happens when switching to the shortest path. The calling
	     * function will immediately call k_chg_mfc() to modify the
	     * kernel cache.
	     */
	    if (!(flags & MFC_MOVE_FORCE))
		return;
	}

	/* Find the exact entry */

	found = FALSE;
	for (node = mrtentry_rp->kernel_cache; node != NULL; node = node->next)
	{
	    if (inet6_lessthan(&node->group, &mrtentry_ptr->group->group))
		continue;
	    if (inet6_greaterthan(&node->group, &mrtentry_ptr->group->group))
		break;
	    if (inet6_lessthan(&node->source, &mrtentry_ptr->source->address))
		continue;
	    if (inet6_greaterthan(&node->source, &mrtentry_ptr->source->address))
		break;

	    /* We found it! */
	    if (node->prev != NULL)
		node->prev->next = node->next;
	    else
		mrtentry_rp->kernel_cache = node->next;

	    if (node->next != NULL)
		node->next->prev = node->prev;

	    found = TRUE;
	    break;
	}

	if (found == TRUE)
	{
	    if (mrtentry_rp->kernel_cache == NULL)
		mrtentry_rp->flags &= ~(MRTF_KERNEL_CACHE | MRTF_MFC_CLONE_SG);
	    if (mrtentry_ptr->kernel_cache != NULL)
		free(mrtentry_ptr->kernel_cache);
	    mrtentry_ptr->flags |= MRTF_KERNEL_CACHE;
	    mrtentry_ptr->kernel_cache = node;
	    node->prev = NULL;
	    node->next = NULL;
	}
    }
}
