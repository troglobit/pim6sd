SUBDIRS=.
sbin_PROGRAMS= pim6dd
pim6dd_SOURCES=	mld6.c mld6_proto.c inet6.c kern.c main.c config.c debug.c vers.c callout.c route.c routesock.c vif.c timer.c mrt.c pim6.c pim6_proto.c trace.c netlink.c
man5_MANS=pim6dd.conf.5
man8_MANS=pim6dd.8
noinst_HEADERS= debug.h defs.h mld6.h mrt.h pathnames.h pimdd.h trace.h vif.h mroute6.h
EXTRA_DIST=${man8_MANS} ${man5_MANS} LICENSE.mrouted LICENSE.pimd
