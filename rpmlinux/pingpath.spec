
# SPEC for rpmbuild

%define gtag d3da356
%define subversion %(echo "$(git rev-list --count %{gtag}...HEAD)_$(git rev-parse --short HEAD)")

Name:       pingpath
Version:    0.3.%{subversion}
Release:    1
Summary:    'ping' wrapper to display path
License:    GPL-2.0-or-later

Requires: iputils, gtk4, json-glib, libglvnd-opengl, libepoxy, cglm
BuildRequires: (gcc or clang), make, pkgconf, gtk4-devel, json-glib-devel, libglvnd-devel, libepoxy-devel, cglm-devel

%description
Network diagnostic tool based on parsing ping output with some functionality of traceroute.
Written using GTK framework.

%define srcdir %{name}

%prep
rm -rf %{srcdir}
git clone --depth=1 https://github.com/yvs2014/pingpath

%build
cd %{srcdir}
PREFIX=/usr DESTDIR=%{buildroot} make

%install
cd %{srcdir}
PREFIX=/usr DESTDIR=%{buildroot} make install

%files
/usr/bin/pingpath
/usr/bin/pingpath3
/usr/share/applications/net.tools.pingpath.desktop
/usr/share/icons/hicolor/scalable/apps/pingpath.svg
/usr/share/doc/pingpath/examples/pingpath.conf
/usr/share/man/man1/pingpath.1.gz

%changelog
# skip this for now

