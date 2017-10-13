#
# SMC Tools - Shared Memory Communication Tools
#
# Copyright IBM Corp. 2016, 2017
#
# All rights reserved. This program and the accompanying materials
# are made available under the terms of the Eclipse Public License v1.0
# which accompanies this distribution, and is available at
# http://www.eclipse.org/legal/epl-v10.html
#

SMC_TOOLS_RELEASE = 1.x.x
VER_MAJOR         = $(shell echo $(SMC_TOOLS_RELEASE) | cut -d '.' -f 1)

ARCHTYPE = $(shell uname -m)
ARCH := $(shell getconf LONG_BIT)

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

STUFF_32BIT	  = 0
#
# Check that 31/32-bit build tools are available.
#
ifeq ($(ARCH),64)
LIBDIR		= ${PREFIX}/lib64
ifneq ("$(wildcard ${PREFIX}/include/gnu/stubs-32.h)","")
STUFF_32BIT = 1
LIBDIR32	= ${PREFIX}/lib
endif
else
LIBDIR		= ${PREFIX}/lib
endif

all: ld_pre_smc.so.$(SMC_TOOLS_RELEASE) ld_pre_smc32.so.$(SMC_TOOLS_RELEASE) \
     smcss smc_pnet smc_run README.smctools af_smc.7

CFLAGS ?= -Wall -O3 -g
ALL_CFLAGS = -DSMC_TOOLS_RELEASE=$(SMC_TOOLS_RELEASE) $(CFLAGS)

ifeq ($(ARCHTYPE),s390x)
	MACHINE_OPT32="-m31"
else
	MACHINE_OPT32="-m32"
endif

smc_run: smc_run.in
	my_installdir=${PREFIX} $(GEN) -e "s#@install_dir@#$$my_installdir#g" < $< > $@
	chmod a+x $@

smc-tools.spec: smc-tools.spec.in
	$(GEN) -e "s#x.x.x#$(SMC_TOOLS_RELEASE)#g" < $< > $@

%: %.in	smc-tools.spec
	$(GEN) -e "s#x.x.x#$(SMC_TOOLS_RELEASE)#g" < $< > $@

ld_pre_smc.o: ld_pre_smc.c
	${CCC} ${CFLAGS} -fPIC -c ld_pre_smc.c

ld_pre_smc.so.$(SMC_TOOLS_RELEASE): ld_pre_smc.o
	${LINK} -shared ld_pre_smc.o -ldl -Wl,-z,defs,-soname,$@.$(VER_MAJOR) -o $@

ld_pre_smc32.so.$(SMC_TOOLS_RELEASE): ld_pre_smc.c
ifeq ($(ARCH),64)
ifeq ($(STUFF_32BIT),1)
	${CCC} ${CFLAGS} -fPIC -c ${MACHINE_OPT32} $< -o ld_pre_smc32.o
	${LINK} -shared ld_pre_smc32.o ${MACHINE_OPT32} -ldl -Wl,-soname,$@.$(VER_MAJOR) -o $@
else
	$(warning "Warning: Skipping 31/32-bit library build because 31/32-bit build tools")
	$(warning "         are unavailable. SMC will not support 31/32 bit applications")
	$(warning "         unless the glibc devel package for the appropriate addressing")
	$(warning "         mode is installed and the preload libraries are rebuilt.")
endif
endif

smcss.o: smcss.c smc_diag.h smctools_common.h
	${CCC} ${ALL_CFLAGS} -c smcss.c

smcss: smcss.o
	${CCC} $< -o $@
#
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
	${CCC} ${ALL_CFLAGS} ${SMC_PNET_CFLAGS} -o $@ $< ${SMC_PNET_LFLAGS}

install: all
	echo "  INSTALL"
	install -d -m755 $(DESTDIR)$(LIBDIR) $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR)/man7 $(DESTDIR)$(MANDIR)/man8
	install -s $(INSTALL_FLAGS_BIN) ld_pre_smc.so.$(SMC_TOOLS_RELEASE) $(DESTDIR)$(LIBDIR)
	ln -srf $(DESTDIR)$(LIBDIR)/ld_pre_smc.so.$(SMC_TOOLS_RELEASE) $(DESTDIR)$(LIBDIR)/ld_pre_smc.so.$(VER_MAJOR)
	ln -srf $(DESTDIR)$(LIBDIR)/ld_pre_smc.so.$(SMC_TOOLS_RELEASE) $(DESTDIR)$(LIBDIR)/ld_pre_smc.so
ifeq ($(STUFF_32BIT),1)
	install -d -m755 $(DESTDIR)$(LIBDIR32)
	install -s $(INSTALL_FLAGS_BIN) ld_pre_smc32.so.$(SMC_TOOLS_RELEASE) $(DESTDIR)$(LIBDIR32)/ld_pre_smc.so.$(SMC_TOOLS_RELEASE)
	ln -srf $(DESTDIR)$(LIBDIR32)/ld_pre_smc.so.$(SMC_TOOLS_RELEASE) $(DESTDIR)$(LIBDIR32)/ld_pre_smc.so.$(VER_MAJOR)
	ln -srf $(DESTDIR)$(LIBDIR32)/ld_pre_smc.so.$(SMC_TOOLS_RELEASE) $(DESTDIR)$(LIBDIR32)/ld_pre_smc.so
endif
	install $(INSTALL_FLAGS_BIN) smc_run $(DESTDIR)$(BINDIR)
	install -s $(INSTALL_FLAGS_BIN) smcss $(DESTDIR)$(BINDIR)
	install -s $(INSTALL_FLAGS_BIN) smc_pnet $(DESTDIR)$(BINDIR)
	install $(INSTALL_FLAGS_MAN) af_smc.7 $(DESTDIR)$(MANDIR)/man7
	install $(INSTALL_FLAGS_MAN) smc_run.8 $(DESTDIR)$(MANDIR)/man8
	install $(INSTALL_FLAGS_MAN) smc_pnet.8 $(DESTDIR)$(MANDIR)/man8
	install $(INSTALL_FLAGS_MAN) smcss.8 $(DESTDIR)$(MANDIR)/man8

clean:
	echo "  CLEAN"
	rm -f *.o *.so.$(SMC_TOOLS_RELEASE) smc_run smcss smc_pnet README.smctools af_smc.7 smc-tools.spec
