
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

SMC_TOOLS_RELEASE = 1.3.99
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

all: libsmc-preload.so libsmc-preload32.so smc smcss smc_pnet

CFLAGS ?= -Wall -O3 -g
ALL_CFLAGS = -DSMC_TOOLS_RELEASE=$(SMC_TOOLS_RELEASE) $(CFLAGS)

ifeq ($(ARCHTYPE),s390x)
	MACHINE_OPT32="-m31"
else
	MACHINE_OPT32="-m32"
endif

%: %.in
	$(GEN) -e "s#x.x.x#$(SMC_TOOLS_RELEASE)#g" < $< > $@

util.o: util.c  util.h
	${CCC} ${CFLAGS} -c util.c

libutil.a: util.o
	ar rcs libutil.a util.o

libnetlink.o: libnetlink.c  libnetlink.h
	${CCC} ${CFLAGS} -c libnetlink.c

libnetlink.a: libnetlink.o
	ar rcs libnetlink.a libnetlink.o

smc-preload.o: smc-preload.c
	${CCC} ${CFLAGS} -fPIC -c smc-preload.c

libsmc-preload.so: smc-preload.o
	${LINK} ${LDFLAGS} -shared smc-preload.o -ldl -Wl,-z,defs,-soname,$@.$(VER_MAJOR) -o $@
	chmod u+s $@

libsmc-preload32.so: smc-preload.c
ifeq ($(ARCH),64)
ifeq ($(STUFF_32BIT),1)
	${CCC} ${CFLAGS} -fPIC -c ${MACHINE_OPT32} $< -o smc-preload32.o
	${LINK} ${LDFLAGS} -shared smc-preload32.o ${MACHINE_OPT32} -ldl -Wl,-soname,$@.$(VER_MAJOR) -o $@
	chmod u+s $@
else
	$(warning "Warning: Skipping 31/32-bit library build because 31/32-bit build tools")
	$(warning "         are unavailable. SMC will not support 31/32 bit applications")
	$(warning "         unless the glibc devel package for the appropriate addressing")
	$(warning "         mode is installed and the preload libraries are rebuilt.")
endif
endif

ifneq ($(shell sh -c 'command -v pkg-config'),)
SMC_PNET_CFLAGS = $(shell pkg-config --silence-errors --cflags libnl-genl-3.0)
SMC_PNET_LFLAGS = $(shell pkg-config --silence-errors --libs libnl-genl-3.0)
else
SMC_PNET_CFLAGS = -I /usr/include/libnl3
SMC_PNET_LFLAGS = -lnl-genl-3 -lnl-3
endif

dev.o: dev.c
	${CCC} ${ALL_CFLAGS} -c $< -o $@

linkgroup.o: linkgroup.c
	${CCC} ${ALL_CFLAGS} -c $< -o $@

smc.o: smc.c
	${CCC} ${ALL_CFLAGS} -c $< -o $@

smc: smc.o dev.o linkgroup.o libnetlink.a libutil.a
	${CCC} ${ALL_CFLAGS} ${LDFLAGS} -L. -lnetlink -lutil $^ -o $@

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
	${CCC} ${ALL_CFLAGS} ${SMC_PNET_CFLAGS} ${LDFLAGS} -o $@ $< ${SMC_PNET_LFLAGS}

smcss: smcss.c smctools_common.h
	${CCC} ${ALL_CFLAGS} ${LDFLAGS} $< -o $@

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
	install $(INSTALL_FLAGS_BIN) smc $(DESTDIR)$(BINDIR)
	install $(INSTALL_FLAGS_BIN) smcss $(DESTDIR)$(BINDIR)
	install $(INSTALL_FLAGS_BIN) smc_pnet $(DESTDIR)$(BINDIR)
	install $(INSTALL_FLAGS_BIN) smc_dbg $(DESTDIR)$(BINDIR)
ifeq ($(shell uname -m | cut -c1-4),s390)
	install $(INSTALL_FLAGS_BIN) smc_rnics $(DESTDIR)$(BINDIR)
	install $(INSTALL_FLAGS_MAN) smc_rnics.8 $(DESTDIR)$(MANDIR)/man8
endif
	install $(INSTALL_FLAGS_MAN) af_smc.7 $(DESTDIR)$(MANDIR)/man7
	install $(INSTALL_FLAGS_MAN) smc_run.8 $(DESTDIR)$(MANDIR)/man8
	install $(INSTALL_FLAGS_MAN) smc_pnet.8 $(DESTDIR)$(MANDIR)/man8
	install $(INSTALL_FLAGS_MAN) smcss.8 $(DESTDIR)$(MANDIR)/man8
	install $(INSTALL_FLAGS_MAN) smc.8 $(DESTDIR)$(MANDIR)/man8
	install $(INSTALL_FLAGS_MAN) smc-linkgroup.8 $(DESTDIR)$(MANDIR)/man8
	install $(INSTALL_FLAGS_MAN) smc-device.8 $(DESTDIR)$(MANDIR)/man8
ifneq ($(BASH_AUTODIR),)
	install $(INSTALL_FLAGS_MAN) smc-tools.autocomplete $(DESTDIR)$(BASH_AUTODIR)/smc-tools
	ln -sfr $(DESTDIR)$(BASH_AUTODIR)/smc-tools $(DESTDIR)$(BASH_AUTODIR)/smc_rnics
	ln -sfr $(DESTDIR)$(BASH_AUTODIR)/smc-tools $(DESTDIR)$(BASH_AUTODIR)/smc_dbg
	ln -sfr $(DESTDIR)$(BASH_AUTODIR)/smc-tools $(DESTDIR)$(BASH_AUTODIR)/smcss
	ln -sfr $(DESTDIR)$(BASH_AUTODIR)/smc-tools $(DESTDIR)$(BASH_AUTODIR)/smc_pnet
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
	rm -f *.o *.so *.a smc smcss smc_pnet
