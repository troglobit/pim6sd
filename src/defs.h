/*	$KAME: defs.h,v 1.5 2001/08/09 10:10:06 suz Exp $	*/

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


#ifndef DEFS_H
#define DEFS_H

#ifdef HAVE_CONFIG_H
#include "../include/config.h"
#endif

#include <sys/param.h>		/* Defines BSD system version (year & month) */

/*
 * Various definitions to make it working for different platforms
 */
/* The old style sockaddr definition doesn't have sa_len */
#if defined(_AIX) || (defined(BSD) && (BSD >= 199006)) /* sa_len was added with 4.3-Reno */
#define HAVE_SA_LEN
#endif

#define	TRUE		1
#define	FALSE		0
#define ELSE		else           /* To make emacs cc-mode happy */

/* From The Practice of Programming, by Kernighan and Pike */
#ifndef NELEMS
#define NELEMS(array)	(sizeof(array) / sizeof(array[0]))
#endif
#define EQUAL(s1, s2)	((strlen(s1) == strlen(s2)) && (strcmp((s1), (s2)) == 0))
#define	max(a, b)	((a) < (b) ? (b) : (a))

typedef	void	( *ihfunc_t )		 ( int , fd_set * ) ;
typedef	void	( *cfunc_t )		 ( void * ) ;

int register_input_handler (int fd,ihfunc_t func);

/*  CONFIGCONFIGCONFIGCONFIG */
void config_vifs_from_kernel (void);
void config_vifs_from_file (void);

#define RANDOM()	random()

#define PRINTF printf
#define ALL_MCAST_GROUPS_LENGTH 8 


typedef u_int32_t	u_int32;
typedef u_int16_t	u_int16;
typedef u_int8_t	u_int8;


extern char configfilename[];

#ifndef strlcpy
size_t  strlcpy    (char *dst, const char *src, size_t siz);
#endif

#ifndef strlcat
size_t  strlcat    (char *dst, const char *src, size_t siz);
#endif

#endif /* DEFS_H */
