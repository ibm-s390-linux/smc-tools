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

SMC_TOOLS_RELASE	= 1.0.0

ARCH = $(shell uname -m)
ifeq ($(ARCH),i686)
ARCH64=0
else ifeq ($(ARCH),s390)
ARCH64=0
else
ARCH64=1
endif

DESTDIR        ?=
PREFIX          = /usr
BINDIR		= ${PREFIX}/bin
MANDIR		= ${PREFIX}/share/man
OWNER		= $(shell id -un)
GROUP		= $(shell id -gn)
INSTALL_FLAGS_BIN	= -g $(GROUP) -o $(OWNER) -m755
INSTALL_FLAGS_MAN	= -g $(GROUP) -o $(OWNER) -m644

STUFF_32BIT	= 0
#
# Check that 31/32-bit build tools are available.
#
ifeq ($(ARCH64),1)
LIBDIR		= ${PREFIX}/lib64
ifneq ("$(wildcard ${PREFIX}/include/gnu/stubs-32.h)","")
STUFF_32BIT = 1
LIBDIR32	= ${PREFIX}/lib
endif
else
LIBDIR		= ${PREFIX}/lib
endif

all: smc_run ld_pre_smc.so ld_pre_smc32.so smcss smc_pnet

CFLAGS := -Wall -I include -O3 -g

ifeq ($(ARCH),s390x)
	MACHINE_OPT32="-m31"
else
	MACHINE_OPT32="-m32"
endif

smc_run: smc_run.in
	my_installdir=${PREFIX}; \
        sed -e "s#@install_dir@#$$my_installdir#g" < $< > $@
	chmod a+x $@

ld_pre_smc.so: ld_pre_smc.c
	${CC} ${CFLAGS} -fPIC -c ld_pre_smc.c
	${CC} -shared ld_pre_smc.o -ldl -Wl,-z,defs -o ld_pre_smc.so

ld_pre_smc32.so: ld_pre_smc.c
ifeq ($(ARCH64),1)
ifeq ($(STUFF_32BIT),1)
	${CC} ${CFLAGS} -fPIC -c ${MACHINE_OPT32} ld_pre_smc.c -o ld_pre_smc32.o
	${CC} -shared ld_pre_smc32.o ${MACHINE_OPT32} -ldl -o ld_pre_smc32.so
else
	$(warning "Warning: Skipping 31/32-bit library build because 31/32-bit \
build tools are unavailable. SMC-R will not support 31 and 32 bit TCP \
applications unless the glibc-devel for appropriate addressing mode is \
installed and the preload libraries are rebuilt.")
endif
endif

smcss.o: smcss.c smc_diag.h smctools_common.h
	${CC} ${CFLAGS} -c smcss.c

smcss: smcss.o

#
ifneq ($(shell sh -c 'command -v pkg-config'),)
SMC_PNET_CFLAGS = $(shell pkg-config --silence-errors --cflags libnl-genl-3.0)
SMC_PNET_LFLAGS = $(shell pkg-config --silence-errors --libs libnl-genl-3.0)
else
SMC_PNET_CFLAGS = -I/usr/include/libnl3
SMC_PNET_LFLAGS = -lnl-genl-3 -lnl-3
endif

smc_pnet: smc_pnet.c smc.h
	@if [ -e /usr/include/libnl3/netlink/netlink.h ]; then \
		echo ${CC} ${CFLAGS} ${SMC_PNET_CFLAGS} -o $@ $< ${SMC_PNET_LFLAGS}; \
		${CC} ${CFLAGS} ${SMC_PNET_CFLAGS} -o $@ $< ${SMC_PNET_LFLAGS}; \
	else \
		printf "*********************************************\n" >&2; \
		printf "* Missing build requirement for: %-45s\n" $@ >&2; \
		printf "* Install package..............: %-45s\n" libnl3-devel >&2; \
		printf "*********************************************\n" >&2; \
		exit 0; \
	fi

install:
	install -d -m755 $(DESTDIR)$(LIBDIR) $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR)/man7 $(DESTDIR)$(MANDIR)/man8
	install -s $(INSTALL_FLAGS_BIN) ld_pre_smc.so $(DESTDIR)$(LIBDIR)
ifeq ($(STUFF_32BIT),1)
	install -d -m755 $(DESTDIR)$(LIBDIR32)
	install -s $(INSTALL_FLAGS_BIN) ld_pre_smc32.so $(DESTDIR)$(LIBDIR32)/ld_pre_smc.so
endif
	install $(INSTALL_FLAGS_BIN) smc_run $(DESTDIR)$(BINDIR)
	install -s $(INSTALL_FLAGS_BIN) smcss $(DESTDIR)$(BINDIR)
	install -s $(INSTALL_FLAGS_BIN) smc_pnet $(DESTDIR)$(BINDIR)
	install $(INSTALL_FLAGS_MAN) af_smc.7 $(DESTDIR)$(MANDIR)/man7
	install $(INSTALL_FLAGS_MAN) smc_run.8 $(DESTDIR)$(MANDIR)/man8
	install $(INSTALL_FLAGS_MAN) smc_pnet.8 $(DESTDIR)$(MANDIR)/man8
	install $(INSTALL_FLAGS_MAN) smcss.8 $(DESTDIR)$(MANDIR)/man8

clean:
	rm -f *.o *.so smc_run smcss smc_pnet
