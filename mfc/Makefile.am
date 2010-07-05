SUBDIRS=.
sbin_PROGRAMS= mfc
mfc_SOURCES= mfc.c cfparse.y cftoken.l 
man8_MANS= mfc.8
mfc_LDADD=-ly
noinst_HEADERS=mfc.h cfparse.h 
EXTRA_DIST = ${man8_MANS} mfc.conf.sample
DISTCLEANFILES = cftoken.c cfparse.c cfparse.h

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
