
# SPEC for rpmbuild

%define gtag d3da356
%define subversion %(echo "$(git rev-list --count %{gtag}...HEAD)_$(git rev-parse --short HEAD)")

Name:       pingpath
Version:    0.3.%{subversion}
Release:    1
Summary:    'ping' wrapper to display path
License:    GPL-2.0-or-later

Requires: iputils, (gtk4 or libgtk-4-1), json-glib, (libglvnd-opengl or libglvnd), (libepoxy or libepoxy0), libcglm0
BuildRequires: (gcc or clang), make, pkgconf, gtk4-devel, json-glib-devel, libglvnd-devel, libepoxy-devel, cglm-devel

%description
Network diagnostic tool based on parsing ping output with some functionality of traceroute.
Written using GTK framework.

%define srcdir %{name}
%define prefix /usr
%define bindir %{prefix}/bin
%define datdir %{prefix}/share
%define dskdir %{datdir}/applications
%define docdir %{datdir}/doc/%{name}
%define mandir %{datdir}/man/man1

%prep
rm -rf %{srcdir}
git clone --depth=1 https://github.com/yvs2014/%{name}

%build
cd %{srcdir}
CFLAGS="${CFLAGS} -fPIE" LDFLAGS="${LDFLAGS} -pie" PREFIX=%{prefix} DESTDIR=%{buildroot} make

%install
cd %{srcdir}
PREFIX=%{prefix} DESTDIR=%{buildroot} make install

%files
%{bindir}/%{name}
%{bindir}/%{name}2
%{dskdir}/net.tools.%{name}.desktop
%{dskdir}/net.tools.%{name}2.desktop
%{datdir}/icons/hicolor/scalable/apps/%{name}.svg
%{docdir}/examples/%{name}.conf
%{mandir}/%{name}.1.gz
%{mandir}/%{name}2.1.gz

%changelog
# skip this for now

