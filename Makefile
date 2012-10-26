VERSION = 0.59
ISSUSE	= -DSUSE

INSTALL		= install -m 755
INSTALL_DATA	= install -m 644
DESTDIR		=
sbindir		= /sbin
mandir		= /usr/share/man
man8dir		= $(mandir)/man8

SRCS		= startpar.c makeboot.c proc.c
HDRS		= makeboot.h proc.h
REST		= COPYING Makefile startpar.8
OBJS		= $(SRCS:.c=.o)

ifneq ($(INC),)
    LIBS	+= -lblogger
    COPTS	+= -DUSE_BLOGD
endif

CC     = gcc
CFLAGS = $(RPM_OPT_FLAGS) $(COPTS) -D_GNU_SOURCE $(INC) -Wall -W -pipe

ifeq ($(MAKECMDGOALS),makeboot)
CFLAGS += -DTEST
endif

.c.o:
	$(CC) $(CFLAGS) -DVERSION=\"$(VERSION)\" $(ISSUSE) -c $<

startpar: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -DVERSION=\"$(VERSION)\" $(ISSUSE) -o $@ $(OBJS) $(LIBS)

makeboot: makeboot.c

install: startpar
	$(INSTALL) -d $(DESTDIR)$(sbindir) $(DESTDIR)$(man8dir)
	$(INSTALL) startpar $(DESTDIR)$(sbindir)/.
	$(INSTALL_DATA) startpar.8 $(DESTDIR)$(man8dir)/.

clean:
	rm -f startpar makeboot $(OBJS)

dest: clean
	mkdir -p startpar-$(VERSION)
	for file in $(SRCS) $(HDRS) $(REST) ; do \
	    cp -p $$file startpar-$(VERSION)/; \
	done
	tar -cps -jf startpar-$(VERSION).tar.bz2 startpar-$(VERSION)/*
	rm -rf startpar-$(VERSION)/
