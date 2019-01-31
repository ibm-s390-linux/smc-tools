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

SMC_TOOLS_RELEASE = 1.1.0
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
LIBDIR		= ${PREFIX}/lib/s390x-linux-gnu
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

all: libsmc-preload.so libsmc-preload32.so smcss smc_pnet smc-tools.spec smc_dbg smc_rnics README.smctools af_smc.7

CFLAGS ?= -Wall -O3 -g
ALL_CFLAGS = -DSMC_TOOLS_RELEASE=$(SMC_TOOLS_RELEASE) $(CFLAGS)

ifeq ($(ARCHTYPE),s390x)
	MACHINE_OPT32="-m31"
else
	MACHINE_OPT32="-m32"
endif

smc_rnics: smc_rnics.in
	$(GEN) -e "s#x.x.x#$(SMC_TOOLS_RELEASE)#g" < $< > $@
	chmod a+x $@

smc_dbg: smc_dbg.in
	$(GEN) -e "s#x.x.x#$(SMC_TOOLS_RELEASE)#g" < $< > $@
	chmod a+x $@

%: %.in
	$(GEN) -e "s#x.x.x#$(SMC_TOOLS_RELEASE)#g" < $< > $@

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

smc_pnet: smc_pnet.c smc.h smctools_common.h
	@if [ ! -e /usr/include/libnl3/netlink/netlink.h ]; then \
		printf "**************************************************************\n" >&2; \
		printf "* Missing build requirement for: %-45s\n" $@ >&2; \
		printf "* Install package..............: %-45s\n" "devel package for libnl3" >&2; \
		printf "* Install package..............: %-45s\n" "devel package for libnl3-genl" >&2; \
		printf "* NOTE: Package names might differ by platform\n" >&2; \
		printf "**************************************************************\n" >&2; \
		exit 1; \
	fi
	${CCC} ${ALL_CFLAGS} ${SMC_PNET_CFLAGS} ${LDFLAGS} -o $@ $< ${SMC_PNET_LFLAGS}

smcss: smcss.c smc_diag.h smctools_common.h
	${CCC} ${ALL_CFLAGS} ${LDFLAGS} $< -o $@

install: all
	echo "  INSTALL"
	install -d -m755 $(DESTDIR)$(LIBDIR) $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR)/man7 $(DESTDIR)$(MANDIR)/man8
	install $(INSTALL_FLAGS_LIB) libsmc-preload.so $(DESTDIR)$(LIBDIR)
ifeq ($(STUFF_32BIT),1)
	install -d -m755 $(DESTDIR)$(LIBDIR32)
	install $(INSTALL_FLAGS_LIB) libsmc-preload32.so $(DESTDIR)$(LIBDIR32)/libsmc-preload.so
endif
	install $(INSTALL_FLAGS_BIN) smc_run $(DESTDIR)$(BINDIR)
	install $(INSTALL_FLAGS_BIN) smcss $(DESTDIR)$(BINDIR)
	install $(INSTALL_FLAGS_BIN) smc_pnet $(DESTDIR)$(BINDIR)
	install $(INSTALL_FLAGS_BIN) smc_dbg $(DESTDIR)$(BINDIR)
ifeq ($(shell uname -m | cut -c1-4),s390)
	install $(INSTALL_FLAGS_BIN) smc_rnics $(DESTDIR)$(BINDIR)
endif
	install $(INSTALL_FLAGS_MAN) af_smc.7 $(DESTDIR)$(MANDIR)/man7
	install $(INSTALL_FLAGS_MAN) smc_run.8 $(DESTDIR)$(MANDIR)/man8
	install $(INSTALL_FLAGS_MAN) smc_rnics.8 $(DESTDIR)$(MANDIR)/man8
	install $(INSTALL_FLAGS_MAN) smc_pnet.8 $(DESTDIR)$(MANDIR)/man8
	install $(INSTALL_FLAGS_MAN) smcss.8 $(DESTDIR)$(MANDIR)/man8

clean:
	echo "  CLEAN"
	rm -f *.o *.so smcss smc_pnet README.smctools af_smc.7 smc-tools.spec smc_dbg smc_rnics
