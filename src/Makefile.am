sbin_PROGRAMS     = pim6sd
dist_sbin_SCRIPTS = pim6stat

AM_YFLAGS         = -d
pim6sd_LIBS       = @LIBS@ @LEXLIBS@
pim6sd_CFLAGS     = -std=c99
pim6sd_CPPFLAGS   = -DYIPS_DEBUG -D_GNU_SOURCE -I$(top_srcdir)/include
pim6sd_SOURCES    = callout.c		callout.h			\
		    cftoken.l		cfparse-defs.h			\
		    cfparse.y		cfparse.h			\
		    config.c		defs.h				\
		    crc.c		crc.h				\
		    debug.c		debug.h				\
		    inet6.c		inet6.h				\
		    kern.c		kern.h				\
		    main.c		pathnames.h	pimd.h		\
		    mld6.c		mld6.h				\
		    mld6_proto.c	mld6_proto.h			\
		    mld6v2.c		mld6v2.h			\
		    mld6v2_proto.c	mld6v2_proto.h			\
		    mrt.c		mrt.h				\
		    netlink.c		routesock.c	routesock.h	\
		    pim6.c		pim6.h				\
		    pim6_proto.c	pim6_proto.h			\
		    route.c		route.h				\
		    rp.c		rp.h				\
		    timer.c		timer.h				\
		    trace.c		trace.h				\
		    vif.c		vif.h

BUILT_SOURCES     = cfparse.h
DISTCLEANFILES    = cftoken.c cfparse.c cfparse.h
