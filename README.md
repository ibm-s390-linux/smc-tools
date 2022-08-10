SMC Tools
=========

Utilities for use with `AF_SMC` sockets.

This package consists of the following tools:

- `libsmc-preload.so` : preload library.
- `smc`               : List linkgroups, links, devices, and more
- `smc_chk`           : SMC support diagnostics
- `smc_pnet`          : C program for PNET Table handling
- `smc_rnics`         : List available RDMA NICs
- `smc_run`           : preload library environment setup script.
- `smcss`             : C program for displaying the information about active
                        SMC sockets.

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

* __v1.8.99 (tbd)__

    Changes:

    Bug fixes:
    - `smc_run`: Fix for single quotes in parameters
    - `Makefile`: Fix target `check`

* __v1.8.1 (2022-04-14)__

    Changes:
    - `smc_rncs`: Recognize RoCE Express3 cards

* __v1.8.0 (2022-04-11)__

    Changes:
    - `smc_dbg`: Add stats and `smc info` output
    - `smc_rnics`:
        - List unknown devices with option `-a`
        - Include software-set PNET IDs
    - `smc_chk`: Indicate PNET IDs set by `smc_pnet`.

    Bug fixes:
    - `smc_rnics`: Display correct PNET ID for unknown Mellanox cards
    - `smc_run`: Fix output of version info

* __v1.7.0 (2021-10-29)__

    Changes:
    - Add support for SMC-Rv2
    - `smcd`/`smcr`: Add support for new commands `seid` and `ueid` to
      manage system and user EIDs

* __v1.6.1 (2021-10-01)__

    Bug fixes:
    - `smcd`/`smcr` statistics:
        - Fix memory overread in is_data_consistent()
        - Fix memory and file handle leaks
        - Use correct fallback counter values after reset

* __v1.6.0 (2021-07-01)__

    Changes:
    - `smcd`/`smcr`: Add new command `stats`
    - `smc_rnics`: Recognize unknown Mellanox cards
    - `smc_run`: Add various command-line switches

    Bug fixes:
    - `smc_chk`: Remove 'EXPERIMENTAL' flag
    - `smc_chk`: Improve cleanup
    - `smc_chk`: Start server with intended port
    - `Makefile`: Install `smc_chk.8` on s390 only
    - `Makefile`: Fix extra compile flags handling
    - `smc_rnics`: Handle malformed FID arguments

* __v1.5.0 (2021-01-29)__

    Changes:
    - `smcd`/`smcr`: Add new command `info`
    - `smc_rnics`: Use `n/a` to indicate missing PNET ID
    - `smc_chk`: New tool to perform SMC eligilibilty checks, requires `man` and
               `python3` to be installed
    - `man` pages: Consistency improvements

    Bug fixes:
    - `smc_pnet.8`: Use correct spelling for 'PNET ID'
    - `smc_rnics`: Suppress output of port attribute for offline devices

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
