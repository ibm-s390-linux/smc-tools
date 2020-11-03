SMC Tools
=========

Utilities for use with `AF_SMC` sockets.

This package consists of the tools:

- `libsmc-preload.so` : preload library.
- `smc_run`           : preload library environment setup script.
- `smc_pnet`          : C program for PNET Table handling
- `smcss`             : C program for displaying the information about active
                        SMC sockets.
- `smc_rnics`         : List available RDMA NICs
- `smc`               : List linkgroups, links and devices.

The preload shared library `libsmc-preload.so` provides mapping of TCP socket
operations to SMC sockets.
The environment setup script `smc_run` sets up the preload environment
for the `libsmc-preload.so` shared library before starting application.
The `smcss` program is used to gather and display information about the
SMC sockets.
The `smc_pnet` program is used to create, destroy, and change the SMC-R PNET
table.

In addition the package contains the `AF_SMC` manpage (`man af_smc`).


License
-------
See [LICENSE](LICENSE).


Code Contributions
------------------
See [CONTRIBUTING.md](CONTRIBUTING.md).


Release History:
================

* __v1.4.0 (2020-11-03)__

    Changes:
    - Add SMC-Dv2 support
    - `smc`: Add new tools `smcd` and `smcr` to list linkgroups, links and
             devices. Requires Linux kernel 5.11 or higher.
    - `smc_rnics`: Display enabled devices per default, add new option `--all`
    - `smc_rnics`: Sort output by FID

    Bug fixes:
    - `smc_rnics`/`smc_dbg`: Fix PNETID for multiport devices
    - `smcss`/`smc_pnet`: Consistent use of option `-v`

* __v1.3.1 (2020-09-14)__

    Changes:
    - `smcss`:     Add further error codes to man page

    Bug fixes:
    - `smcss`:     Display more than 321 connections
    - `smc_rnics`: Suppress any unknown non-networking device unless
                   option `-r` is specified

* __v1.3.0 (2020-06-16)__

    Changes:
    - `smcss`: Add description of Linux error codes to man page
    - `smc_rnics`:
         * Sort output by PCHID
         * Replace spaces in output by underscores for easier parsing
         * Add new option `--IB-dev` to display IB-specific attributes

    Bug fixes:
    - smc_rnics:
         * FIDs can have up to 4 digits and are planned to be extended
           to a total of 8 digits - adjusting output format accordingly
         * Do not display port attribute for RoCE Express2 devices
           unless we have an accurate value

* __v1.2.2 (2019-10-24)__
    Changes:
    - Add bash autocompletion support
    - `Makefile`: Drop 31 Bit install due to rpmbuild conflict

    Bug fixes:
    - `smcss`: Do not show connection mode for already closed sockets
    - `smc_rnics`: Set interface to "n/a" for ISM devices

* __v1.2.1 (2019-04-15)__
    Bug fixes:
    - `smc_rnics`: Install man page on s390 only
    - `libsmc..`: Handle behavior flags in type argument to `socket()` call
    - `Makefile`: Fixed install target on Ubuntu for platforms other than s390
    - `smc_pnet`: Changes in support of kernel 5.1

* __v1.2.0 (2019-02-08)__
    Changes:
    - `smc_rnics`: Initial version added
    - `smc_dbg`: Initial version added

    Bug fixes:
    - `smcss`: Parse address family of ip address

* __v1.1.0 (2018-06-29)__
    Changes:
    - `smcss`:    Add IPv6 support
    - `libsmc..`: Add IPv6 support
    - `smcss`:    Output format changed
    - `libsmc..`: Rename preload library to `libsmc-preload.so`
    - `Makefile`: Improve distro compatibility
    - `Makefile`: Add `SONAME` to shared libraries
    - `Makefile`: Do not strip binaries on install
    - `Makefile`: Use `LDFLAGS` to allow addition of externally set link flags
    - `libsmc..`: Remove hardcoded reference to libc
    - Manpages:   Formatting changes

    Bug fixes:
    - `Makefile`: Fix target `install` dependencies
    - `smcss`:    Fix `--version` output
    - `smc_pnet`: Fix `--version` output
    - `smc_run`:  Append preload library to `LD_PRELOAD` instead of potentially
                  overwriting pre-set values
    - `libsmc..`: Set suid flag to work with suid executables

* __v1.0.0 (2017-02-13)__
    The initial version


Copyright IBM Corp. 2016, 2020

