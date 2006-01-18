#
# spec file for package SUNWcherokee
#
# Copyright (c) 2006 Sun Microsystems, Inc.
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself.
#

%include Solaris.inc

Name:                    SUNWcherokee
Summary:                 cherokee - Fast, flexible, lightweight web server
Version:                 0.4.30
Source:                  http://www.0x50.org/download/0.4/%{version}/cherokee-%{version}.tar.gz
SUNW_BaseDir:            %{_basedir}
BuildRoot:               %{_tmppath}/%{name}-%{version}-build

%include default-depend.inc
#BuildRequires: SUNWgnome-base-libs-share

%package root
Summary:                 cherokee - Fast, flexible, lightweight web server - platform dependent files, / filesystem
SUNW_BaseDir:            /
%include default-depend.inc

%package devel          
Summary:                 cherokee - Fast, flexible, lightweight web server - developer files
SUNW_BaseDir:            %{_basedir}
%include default-depend.inc
Requires: SUNWcherokee

%prep
%setup -q -n cherokee-%version

%build
#export PKG_CONFIG_PATH=%{_pkg_config_path}
#export MSGFMT="/usr/bin/msgfmt"
export CFLAGS="%optflags -I%{_includedir}"
export RPM_OPT_FLAGS="$CFLAGS"

./configure --prefix=%{_prefix}                 \
            --enable-os-string="OpenSolaris"    \
            --enable-pthreads                   \
            --libexecdir=%{_libexecdir}         \
            --datadir=%{_datadir}               \
            --mandir=%{_mandir}                 \
            --infodir=%{_infodir}               \
            --sysconfdir=%{_sysconfdir}         \
            --localstatedir=%{_localstatedir}   \
            --with-wwwroot=%{_localstatedir}/cherokee
make -j$CPUS

%install
make install DESTDIR=$RPM_BUILD_ROOT

rm -f $RPM_BUILD_ROOT%{_libdir}/lib*.a
rm -f $RPM_BUILD_ROOT%{_libdir}/lib*.la
rm -f $RPM_BUILD_ROOT%{_libdir}/cherokee/lib*.a
rm -f $RPM_BUILD_ROOT%{_libdir}/cherokee/lib*.la

%{?pkgbuild_postprocess: %pkgbuild_postprocess -v -c "%{version}:%{jds_version}:%{name}:$RPM_ARCH:%(date +%%Y-%%m-%%d):%{support_level}" $RPM_BUILD_ROOT}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr (-, root, other)
%dir %attr (0755, root, bin) %{_bindir}
%{_bindir}/*
%dir %attr (0755, root, bin) %{_sbindir}
%{_sbindir}/cherokee
%dir %attr (0755, root, bin) %{_libdir}
%{_libdir}/lib*.so*
%{_libdir}/cherokee
%dir %attr (0755, root, sys) %{_datadir}
%dir %attr(0755, root, bin) %{_mandir}
%dir %attr(0755, root, bin) %{_mandir}/man1
%{_mandir}/man1/*
%{_datadir}/aclocal
%{_datadir}/cherokee
%{_datadir}/doc

%files root
%defattr (-, root, other)
%attr (0755, root, sys) %dir %{_sysconfdir}
%{_sysconfdir}/*
%dir %attr (0755, root, sys) %{_localstatedir}
%{_localstatedir}/*
%defattr (0755, root, sys)

%files devel
%dir %attr (0755, root, bin) %dir %{_libdir}
%dir %attr (0755, root, other) %{_libdir}/pkgconfig
%{_libdir}/pkgconfig/*
%dir %attr (0755, root, bin) %{_includedir}
%{_includedir}/*


%changelog
* Tue Jan 17 2006 - damien.carbery@sun.com
- Created.
