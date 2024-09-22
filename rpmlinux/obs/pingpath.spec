
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
BuildRequires: meson, git, pkgconf, gtk4-devel, json-glib-devel, libglvnd-devel, libepoxy-devel, cglm-devel
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
%define docnam %{_docdir}/%{name}
%define smpdir %{docnam}/examples
%define rdns net.tools.%{name}

%prep
%autosetup

%build
%meson -Ddocdir=%{_docdir}
%meson_build

%install
%meson_install

%files
%defattr(-,root,root,-)
%dir %{docnam}
%dir %{smpdir}
%{bindir}/%{name}
%{dskdir}/%{rdns}.desktop
%{bindir}/%{name}2
%{dskdir}/%{rdns}2.desktop
%{datdir}/icons/hicolor/scalable/apps/%{name}.svg
%{smpdir}/%{name}.conf
%{mandir}/%{name}.1*

%changelog
# autofill
