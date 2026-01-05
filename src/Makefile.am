# For replacement functions in lib/
AUTOMAKE_OPTIONS = subdir-objects

# Tell yacc/bison to also produce a header file
AM_YFLAGS         = -d

sbin_PROGRAMS     = pim6sd
dist_sbin_SCRIPTS = pim6stat

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
		    pim6.c		pim6.h				\
		    pim6_proto.c	pim6_proto.h			\
		    route.c		route.h				\
		    rp.c		rp.h				\
		    timer.c		timer.h				\
		    trace.c		trace.h				\
		    vif.c		vif.h
pim6sd_LDADD      = $(LIBS) $(LIBOBJS)
pim6sd_CFLAGS     = -std=c99 -W -Wall -Wextra -Wno-unused
pim6sd_CPPFLAGS   = -DYIPS_DEBUG -I$(top_srcdir)/include

if LINUX
pim6sd_CPPFLAGS  += -D_GNU_SOURCE
pim6sd_SOURCES   += netlink.c netlink.h
endif

if BSD
pim6sd_SOURCES   += routesock.c routesock.h
endif

# https://www.gnu.org/software/automake/manual/html_node/Sources.html
BUILT_SOURCES     = cfparse.h
DISTCLEANFILES    = cftoken.c cfparse.c cfparse.h
