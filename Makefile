VERSION = 0.59
#ISSUSE	= -DSUSE

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

STARTPAR        := $(shell pwd)/startpar

ifneq ($(INC),)
    LIBS	+= -lblogger
    COPTS	+= -DUSE_BLOGD
endif

CC     = gcc
CFLAGS = $(RPM_OPT_FLAGS) $(COPTS) -D_GNU_SOURCE $(INC) -Wall -W -pipe

# Enable large file support on GNU/Hurd
CFLAGS += $(shell getconf LFS_CFLAGS)

ifeq ($(MAKECMDGOALS),makeboot)
CFLAGS += -DTEST
endif

.c.o:
	$(CC) $(CFLAGS) -DVERSION=\"$(VERSION)\" $(ISSUSE) -c $<

all: startpar
startpar: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -DVERSION=\"$(VERSION)\" $(ISSUSE) -o $@ $(OBJS) $(LIBS)

makeboot: makeboot.c

install: startpar
	$(INSTALL) -d $(DESTDIR)$(sbindir) $(DESTDIR)$(man8dir)
	$(INSTALL) startpar $(DESTDIR)$(sbindir)/.
	$(INSTALL_DATA) startpar.8 $(DESTDIR)$(man8dir)/.

check: all
	$(MAKE) STARTPAR=$(STARTPAR) -C testsuite $@

distclean: clean
clean:
	rm -f startpar makeboot $(OBJS)

ifeq ($(MAKECMDGOALS),upload)
PACKAGE=startpar
PROJECT=sysvinit
SVLOGIN=$(shell svn info | sed -rn '/Repository Root:/{ s|.*//(.*)\@.*|\1|p }')
override TMP:=$(shell mktemp -d $(VERSION).XXXXXXXX)
override TARBALL:=$(TMP)/$(PACKAGE)-$(VERSION).tar.bz2
override SFTPBATCH:=$(TMP)/$(VERSION)-sftpbatch

dist: $(TARBALL) $(TARBALL).sig
	@cp $(TARBALL) .
	@cp $(TARBALL).sig .
	@echo "tarball $(PACKAGE)-$(VERSION).tar.bz2 ready"
	rm -rf $(TMP)

upload: $(SFTPBATCH)
	@sftp -b $< $(SVLOGIN)@dl.sv.nongnu.org:/releases/$(PROJECT)
	rm -rf $(TMP)

$(SFTPBATCH): $(TARBALL).sig
	@echo progress > $@
	@echo put $(TARBALL) >> $@
	@echo chmod 664 $(notdir $(TARBALL)) >> $@
	@echo put $(TARBALL).sig >> $@
	@echo chmod 664 $(notdir $(TARBALL)).sig >> $@
#	@echo rm  $(PACKAGE)-latest.tar.bz2 >> $@
#	@echo symlink $(notdir $(TARBALL)) $(PACKAGE)-latest.tar.bz2 >> $@
	@echo quit >> $@

$(TARBALL).sig: $(TARBALL)
	@gpg -q -ba --use-agent -o $@ $<

$(TARBALL): $(TMP)/$(PACKAGE)-$(VERSION)
	@tar --bzip2 --owner=nobody --group=nogroup -cf $@ -C $(TMP) $(PACKAGE)-$(VERSION)

$(TMP)/$(PACKAGE)-$(VERSION): .svn
	svn export . $@
	@chmod -R a+r,u+w,og-w $@
	@find $@ -type d | xargs -r chmod a+rx,u+w,og-w
endif
