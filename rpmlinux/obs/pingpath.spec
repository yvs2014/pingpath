
# OBS compat

Name:       pingpath
Version:    inherited
Release:    1
Summary:    'ping' wrapper to display path
License:    GPL-2.0-or-later
Group:      Productivity/Networking/Other
URL:        https://github.com/yvs2014/%{name}
Source0:    %{name}-%{version}.tar.gz

Requires: iputils
BuildRequires: cmake, pkgconf, gtk4-devel, json-glib-devel, libglvnd-devel, libepoxy-devel, cglm-devel
BuildRequires: (gcc or clang)
%if 0%{?fedora}
BuildRequires: glibc-langpack-en
%endif

%description
Network diagnostic tool based on parsing ping output with some functionality of traceroute.
Written using GTK4 framework.

%define srcdir %{_sourcedir}/%{name}-%{version}
%define prefix /usr
%define bindir %{prefix}/bin
%define datdir %{prefix}/share
%define dskdir %{datdir}/applications
%define mandir %{_mandir}/man1
%if 0%{?suse_version} == 1500 && 0%{?sle_version} < 150600
%define docdir %{datdir}/doc/%{name}
%else
%define docdir %{_docdir}/%{name}
%endif

%prep
%autosetup

%build
%cmake
%cmake_build

%install
%cmake_install

%files
%defattr(-,root,root,-)
%dir %{docdir}
%dir %{docdir}/examples
%{bindir}/%{name}
%{bindir}/%{name}2
%{dskdir}/net.tools.%{name}.desktop
%{dskdir}/net.tools.%{name}2.desktop
%{datdir}/icons/hicolor/scalable/apps/%{name}.svg
%{docdir}/examples/%{name}.conf
%{mandir}/%{name}.1*

%changelog
# autofill
