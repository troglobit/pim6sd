sbin_PROGRAMS   = pim6sd
dist_sbin_SCRIPTS = pim6stat

pim6sd_SOURCES  = mld6.c mld6_proto.c mld6v2.c mld6v2_proto.c inet6.c kern.c main.c config.c debug.c callout.c route.c vif.c timer.c mrt.c pim6.c pim6_proto.c rp.c crc.c trace.c cfparse.y cftoken.l routesock.c netlink.c
pim6sd_LIBS     = @LIBS@ @LEXLIBS@
pim6sd_CPPFLAGS = -D_GNU_SOURCE -I$(top_srcdir)/include

noinst_HEADERS  = callout.h cfparse-defs.h cfparse.h crc.h debug.h defs.h inet6.h kern.h mld6.h mld6_proto.h mld6v2.h mld6v2_proto.h mrt.h pathnames.h pim6.h pim6_proto.h pimd.h route.h routesock.h rp.h timer.h trace.h var.h vif.h vmbuf.h
DISTCLEANFILES  = cftoken.c cfparse.c cfparse.h

# Config file parser (this is necessary because not all yacc generate y.tab.h)
cftoken.o: cftoken.c cfparse.h
	$(COMPILE) -c -o $@ $<

cfparse.o: cfparse.c
	$(COMPILE) -c -o $@ $<

cftoken.c: cftoken.l
	$(LEX) -ocftoken.c $<

cfparse.h cfparse.c: cfparse.y
	$(YACC) -d $< && \
	mv y.tab.c cfparse.c && \
	mv y.tab.h cfparse.h
