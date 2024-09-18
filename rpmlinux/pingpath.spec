
# rpmbuild -ba pingpath.spec

%define gtag d3da356
%define gver .%(echo "$(git rev-list --count %{gtag}..HEAD)_$(git rev-parse --short HEAD)")

Name:       pingpath
Version:    0.3%{gver}
Release:    1
Summary:    'ping' wrapper to display path
License:    GPL-2.0-or-later
Group:      Productivity/Networking/Other
URL:        https://github.com/yvs2014/%{name}

Requires: iputils
BuildRequires: cmake, pkgconf, gtk4-devel, json-glib-devel, libglvnd-devel, libepoxy-devel, cglm-devel
BuildRequires: (gcc or clang)

%description
Network diagnostic tool based on parsing ping output with some functionality of traceroute.
Written using GTK4 framework.

%define srcdir %{name}
%define prefix /usr
%define bindir %{prefix}/bin
%define datdir %{prefix}/share
%define dskdir %{datdir}/applications
%define docdir %{_docdir}/%{name}
%define mandir %{_mandir}/man1

%prep
rm -rf %{srcdir}
git clone https://github.com/yvs2014/%{name}
#cp -ar %{_sourcedir}/* %{_topdir}/BUILD/ # local test

%build
cd %{srcdir}
%cmake
%cmake_build

%install
cd %{srcdir}
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

