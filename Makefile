#
# SMC Tools - Shared Memory Communication Tools
#
# Copyright IBM Corp. 2016, 2018
#
# All rights reserved. This program and the accompanying materials
# are made available under the terms of the Eclipse Public License v1.0
# which accompanies this distribution, and is available at
# http://www.eclipse.org/legal/epl-v10.html
#

SMC_TOOLS_RELEASE = 1.6.1
VER_MAJOR         = $(shell echo $(SMC_TOOLS_RELEASE) | cut -d '.' -f 1)

ARCHTYPE = $(shell uname -m)
ARCH := $(shell getconf LONG_BIT)
DISTRO := $(shell lsb_release -si 2>/dev/null)

ifneq ("${V}","1")
        MAKEFLAGS += --quiet
        cmd = echo $1$2;
else
        cmd =
endif
CCC               = $(call cmd,"  CC      ",$@)${CC}
LINK              = $(call cmd,"  LINK    ",$@)${CC}
GEN               = $(call cmd,"  GEN     ",$@)sed
DESTDIR          ?=
PREFIX            = /usr
BINDIR		  = ${PREFIX}/bin
MANDIR		  = ${PREFIX}/share/man
BASH_AUTODIR	  = $(shell pkg-config --variable=completionsdir bash-completion 2>/dev/null)
OWNER		  = $(shell id -un)
GROUP		  = $(shell id -gn)
INSTALL_FLAGS_BIN = -g $(GROUP) -o $(OWNER) -m755
INSTALL_FLAGS_MAN = -g $(GROUP) -o $(OWNER) -m644
INSTALL_FLAGS_LIB = -g $(GROUP) -o $(OWNER) -m4755

STUFF_32BIT	  = 0
#
# Check that 31/32-bit build tools are available.
#
ifeq ($(ARCH),64)
ifeq ($(DISTRO),Ubuntu)
LIBDIR		= ${PREFIX}/lib/${ARCHTYPE}-linux-gnu
else
LIBDIR		= ${PREFIX}/lib64
endif
ifneq ("$(wildcard ${PREFIX}/include/gnu/stubs-32.h)","")
STUFF_32BIT = 1
LIBDIR32	= ${PREFIX}/lib
endif
else
ifeq ($(DISTRO),Ubuntu)
LIBDIR		= ${PREFIX}/lib/s390-linux-gnu
else
LIBDIR		= ${PREFIX}/lib
endif
endif

all: libsmc-preload.so libsmc-preload32.so smcd smcr smcss smc_pnet

CFLAGS ?= -Wall -O3 -g
ifneq ($(shell sh -c 'command -v pkg-config'),)
LIBNL_CFLAGS = $(shell pkg-config --silence-errors --cflags libnl-genl-3.0)
LIBNL_LFLAGS = $(shell pkg-config --silence-errors --libs libnl-genl-3.0)
else
LIBNL_CFLAGS = -I /usr/include/libnl3
LIBNL_LFLAGS = -lnl-genl-3 -lnl-3
endif
ALL_CFLAGS += ${CFLAGS} -DSMC_TOOLS_RELEASE=$(SMC_TOOLS_RELEASE) \
              ${LIBNL_CFLAGS} ${OPTFLAGS}
ALL_LDFLAGS += ${LDFLAGS} ${LIBNL_LFLAGS} -lm

ifeq ($(ARCHTYPE),s390x)
	MACHINE_OPT32="-m31"
else
	MACHINE_OPT32="-m32"
endif

util.o: util.c  util.h
	${CCC} ${ALL_CFLAGS} -c util.c

libnetlink.o: libnetlink.c  libnetlink.h
	${CCC} ${ALL_CFLAGS} ${ALL_LDFLAGS} -c libnetlink.c

smc-preload.o: smc-preload.c
	${CCC} ${ALL_CFLAGS} -fPIC -c smc-preload.c

libsmc-preload.so: smc-preload.o
	${LINK} ${ALL_LDFLAGS} -shared smc-preload.o -ldl -Wl,-z,defs,-soname,$@.$(VER_MAJOR) -o $@
	chmod u+s $@

libsmc-preload32.so: smc-preload.c
ifeq ($(ARCH),64)
ifeq ($(STUFF_32BIT),1)
	${CCC} ${ALL_CFLAGS} -fPIC -c ${MACHINE_OPT32} $< -o smc-preload32.o
	${LINK} ${ALL_LDFLAGS} -shared smc-preload32.o ${MACHINE_OPT32} -ldl -Wl,-soname,$@.$(VER_MAJOR) -o $@
	chmod u+s $@
else
	$(warning "Warning: Skipping 31/32-bit library build because 31/32-bit build tools")
	$(warning "         are unavailable. SMC will not support 31/32 bit applications")
	$(warning "         unless the glibc devel package for the appropriate addressing")
	$(warning "         mode is installed and the preload libraries are rebuilt.")
endif
endif


%d.o: %.c
	${CCC} ${ALL_CFLAGS} -DSMCD -c $< -o $@

%r.o: %.c
	${CCC} ${ALL_CFLAGS} -DSMCR -c $< -o $@

%.o: %.c
	${CCC} ${ALL_CFLAGS} -c $< -o $@

smc: smc.o dev.o linkgroup.o libnetlink.o util.o
	${CCC} ${ALL_CFLAGS} ${ALL_LDFLAGS} $^ -o $@

smcd: smcd.o infod.o devd.o linkgroupd.o statsd.o libnetlink.o util.o
	${CCC} ${ALL_CFLAGS} $^ ${ALL_LDFLAGS} -o $@

smcr: smcr.o infor.o devr.o linkgroupr.o statsr.o libnetlink.o util.o
	${CCC} ${ALL_CFLAGS} $^ ${ALL_LDFLAGS} -o $@

smc_pnet: smc_pnet.c smctools_common.h
	@if [ ! -e /usr/include/libnl3/netlink/netlink.h ]; then \
		printf "**************************************************************\n" >&2; \
		printf "* Missing build requirement for: %-45s\n" $@ >&2; \
		printf "* Install package..............: %-45s\n" "devel package for libnl3" >&2; \
		printf "* Install package..............: %-45s\n" "devel package for libnl3-genl" >&2; \
		printf "* NOTE: Package names might differ by platform\n" >&2; \
		printf "*       On Ubuntu try libnl-3-dev and libnl-genl-3-dev\n" >&2; \
		printf "**************************************************************\n" >&2; \
		exit 1; \
	fi
	${CCC} ${ALL_CFLAGS} $< ${ALL_LDFLAGS} -o $@

smcss: smcss.o libnetlink.o
	${CCC} ${ALL_CFLAGS} $^ ${ALL_LDFLAGS} -o $@

install: all
	echo "  INSTALL"
	install -d -m755 $(DESTDIR)$(LIBDIR) $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR)/man7 \
	                 $(DESTDIR)$(BASH_AUTODIR) $(DESTDIR)$(MANDIR)/man8
	install $(INSTALL_FLAGS_LIB) libsmc-preload.so $(DESTDIR)$(LIBDIR)
#ifeq ($(STUFF_32BIT),1)
#	install -d -m755 $(DESTDIR)$(LIBDIR32)
#	install $(INSTALL_FLAGS_LIB) libsmc-preload32.so $(DESTDIR)$(LIBDIR32)/libsmc-preload.so
#endif
	install $(INSTALL_FLAGS_BIN) smc_run $(DESTDIR)$(BINDIR)
	install $(INSTALL_FLAGS_BIN) smcd $(DESTDIR)$(BINDIR)
	install $(INSTALL_FLAGS_BIN) smcr $(DESTDIR)$(BINDIR)
	install $(INSTALL_FLAGS_BIN) smcss $(DESTDIR)$(BINDIR)
	install $(INSTALL_FLAGS_BIN) smc_pnet $(DESTDIR)$(BINDIR)
	install $(INSTALL_FLAGS_BIN) smc_dbg $(DESTDIR)$(BINDIR)
ifeq ($(shell uname -m | cut -c1-4),s390)
	install $(INSTALL_FLAGS_BIN) smc_rnics $(DESTDIR)$(BINDIR)
	install $(INSTALL_FLAGS_MAN) smc_rnics.8 $(DESTDIR)$(MANDIR)/man8
	install $(INSTALL_FLAGS_BIN) smc_chk $(DESTDIR)$(BINDIR)
	install $(INSTALL_FLAGS_MAN) smc_chk.8 $(DESTDIR)$(MANDIR)/man8
endif
	install $(INSTALL_FLAGS_MAN) af_smc.7 $(DESTDIR)$(MANDIR)/man7
	install $(INSTALL_FLAGS_MAN) smc_run.8 $(DESTDIR)$(MANDIR)/man8
	install $(INSTALL_FLAGS_MAN) smc_pnet.8 $(DESTDIR)$(MANDIR)/man8
	install $(INSTALL_FLAGS_MAN) smcss.8 $(DESTDIR)$(MANDIR)/man8
	install $(INSTALL_FLAGS_MAN) smcd.8 $(DESTDIR)$(MANDIR)/man8
	install $(INSTALL_FLAGS_MAN) smcr.8 $(DESTDIR)$(MANDIR)/man8
	install $(INSTALL_FLAGS_MAN) smcd-linkgroup.8 $(DESTDIR)$(MANDIR)/man8
	install $(INSTALL_FLAGS_MAN) smcd-device.8 $(DESTDIR)$(MANDIR)/man8
	install $(INSTALL_FLAGS_MAN) smcd-info.8 $(DESTDIR)$(MANDIR)/man8
	install $(INSTALL_FLAGS_MAN) smcd-stats.8 $(DESTDIR)$(MANDIR)/man8
	ln -sfr $(DESTDIR)$(MANDIR)/man8/smcd-linkgroup.8 $(DESTDIR)$(MANDIR)/man8/smcr-linkgroup.8
	ln -sfr $(DESTDIR)$(MANDIR)/man8/smcd-device.8 $(DESTDIR)$(MANDIR)/man8/smcr-device.8
	ln -sfr $(DESTDIR)$(MANDIR)/man8/smcd-info.8 $(DESTDIR)$(MANDIR)/man8/smcr-info.8
	ln -sfr $(DESTDIR)$(MANDIR)/man8/smcd-stats.8 $(DESTDIR)$(MANDIR)/man8/smcr-stats.8
ifneq ($(BASH_AUTODIR),)
	install $(INSTALL_FLAGS_MAN) smc-tools.autocomplete $(DESTDIR)$(BASH_AUTODIR)/smc-tools
	ln -sfr $(DESTDIR)$(BASH_AUTODIR)/smc-tools $(DESTDIR)$(BASH_AUTODIR)/smc_rnics
	ln -sfr $(DESTDIR)$(BASH_AUTODIR)/smc-tools $(DESTDIR)$(BASH_AUTODIR)/smc_chk
	ln -sfr $(DESTDIR)$(BASH_AUTODIR)/smc-tools $(DESTDIR)$(BASH_AUTODIR)/smc_dbg
	ln -sfr $(DESTDIR)$(BASH_AUTODIR)/smc-tools $(DESTDIR)$(BASH_AUTODIR)/smcss
	ln -sfr $(DESTDIR)$(BASH_AUTODIR)/smc-tools $(DESTDIR)$(BASH_AUTODIR)/smc_pnet
	ln -sfr $(DESTDIR)$(BASH_AUTODIR)/smc-tools $(DESTDIR)$(BASH_AUTODIR)/smc
endif

check:
	if which cppcheck >/dev/null; then \
	    echo "Running cppcheck"; \
	    cppcheck . 2>&1; \
	else \
	    echo "cppcheck not available"; \
	fi
	@echo;
	if which valgrind >/dev/null; then \
	    echo "Running valgrind"; \
	    valgrind --leak-check=full --show-leak-kinds=all ./smcss 2>&1; \
	    valgrind --leak-check=full --show-leak-kinds=all ./smc_pnet 2>&1; \
	    valgrind --leak-check=full --show-leak-kinds=all ./smc 2>&1; \
	else \
	    echo "valgrind not available"; \
	fi
	@echo;
clean:
	echo "  CLEAN"
	rm -f *.o *.so *.a smc smcd smcr smcss smc_pnet
