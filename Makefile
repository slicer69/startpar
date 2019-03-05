VERSION = 0.63
PACKAGE=startpar
PROJECT=sysvinit
#ISSUSE	= -DSUSE

INSTALL		= install -m 755
INSTALL_DATA	= install -m 644
DESTDIR		=
sbindir		= /sbin
mandir		= /usr/share/man
man8dir		= $(mandir)/man8

SRCS		= startpar.c makeboot.c proc.c
CXXSRCS         = compiletest.cc
HDRS		= makeboot.h proc.h
REST		= COPYING Makefile startpar.8
OBJS		= $(SRCS:.c=.o) $(CXXSRCS:.cc=.o)

STARTPAR        := $(shell pwd)/startpar
TARBALL	        = $(PACKAGE)-$(VERSION).tar.xz

ifneq ($(INC),)
    LIBS	+= -lblogger
    COPTS	+= -DUSE_BLOGD
endif

CC     ?= gcc
CFLAGS = $(RPM_OPT_FLAGS) $(COPTS) -D_GNU_SOURCE $(INC) -pipe

WARNINGS = -Wall -W -Wformat -Werror=format-security
CFLAGS += $(WARNINGS)

# Enable large file support on GNU/Hurd
CFLAGS += $(shell getconf LFS_CFLAGS)

CPPFLAGS += $(EXTRACPPFLAGS)
CFLAGS += $(EXTRACFLAGS)
LDFLAGS += $(EXTRALDFLAGS)

CPPFLAGS += $(EXTRACPPFLAGS)
CFLAGS += $(EXTRACFLAGS)
LDFLAGS += $(EXTRALDFLAGS)

ifeq ($(MAKECMDGOALS),makeboot)
CFLAGS += -DTEST
endif

SOURCEFILES= compiletest.cc \
	     CHANGELOG \
             COPYING \
             makeboot.c \
             makeboot.h \
             Makefile \
             proc.c \
             proc.h \
             README \
             startpar.8 \
             startpar.c


.c.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) -DVERSION=\"$(VERSION)\" $(ISSUSE) -c $<

all: startpar
startpar: $(OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -DVERSION=\"$(VERSION)\" $(ISSUSE) -o $@ $(OBJS) $(LIBS)

makeboot: makeboot.c

install: startpar
	$(INSTALL) -d $(DESTDIR)$(sbindir) $(DESTDIR)$(man8dir)
	$(INSTALL) startpar $(DESTDIR)$(sbindir)/.
	$(INSTALL_DATA) startpar.8 $(DESTDIR)$(man8dir)/.

check: all
	$(MAKE) STARTPAR=$(STARTPAR) -C testsuite $@

distclean: clean
	rm -f $(TARBALL) $(TARBALL).sig

clean:
	rm -f startpar makeboot $(OBJS)


dist: $(TARBALL).sig

$(TARBALL).sig: $(TARBALL)
	@gpg -q -ba --use-agent -o $@ $<

$(TARBALL): clean
	mkdir -p startpar/testsuite
	cp $(SOURCEFILES) startpar/
	cp testsuite/* startpar/testsuite/
	@tar --xz --owner=nobody --group=nogroup -cf $(TARBALL) startpar/
	rm -rf startpar/
