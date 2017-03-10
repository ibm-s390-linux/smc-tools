%define with_32bit 0
Name: smc-tools
Version: 1.0.0
Release: 0.ibm
Summary: Shared Memory Communication
Source: %{name}-%{version}.tar.gz
Group: System/Administration
License: Eclipse Public License v1.0
BuildRoot: /var/tmp/%{name}-root
BuildRequires: glibc-devel, libnl3-devel
Requires: libnl3

%description
The smc-tools package enables usage of SMC sockets in Linux.
It contains these tools:
* Preload library "ld_pre_smc.so" together with the calling script "smc_run"
  to trigger transparent usage of SMC-sockets for existing TCP socket applications.
*  Configuration tool called "smc_pnet"
* Tool "smcss" to print information about SMC sockets
* Manpages, among them af_smc.7

%global debug_package %{nil}

%prep
%setup

%build
CFLAGS="%{optflags}" make PREFIX=/usr

%install
rm -rf %{buildroot}
make install PREFIX=%{buildroot}%{_prefix}

%clean
rm -rf %{buildroot}

%files
%defattr (-,root,root)
%doc LICENSE
%doc README.smctools
%{_bindir}/smc_run
%{_bindir}/smc_pnet
%{_bindir}/smcss
%{_mandir}/man7/af_smc.7*
%{_mandir}/man8/smc_run.8*
%{_mandir}/man8/smc_pnet.8*
%{_mandir}/man8/smcss.8*
%{_libdir}/ld_pre_smc.so
%if %{with_32bit}
%define _libdir32="` tr -d '64' < %{_libdir}`"
%{_libdir32}/ld_pre_smc.so
%endif
