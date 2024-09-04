
# rpmbuild -ba pingpath.spec

%define gtag d3da356
%define subversion %(echo "$(git rev-list --count %{gtag}..HEAD)_$(git rev-parse --short HEAD)")

Name:       pingpath
Version:    0.3.%{subversion}
Release:    1
Summary:    'ping' wrapper to display path
License:    GPL-2.0-or-later
Group:      Productivity/Networking/Other
URL:        https://github.com/yvs2014/%{name}

Requires: iputils
BuildRequires: make, pkgconf, gtk4-devel, json-glib-devel, libglvnd-devel, libepoxy-devel, cglm-devel
BuildRequires: (gcc or clang)

%description
Network diagnostic tool based on parsing ping output with some functionality of traceroute.
Written using GTK4 framework.

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
CFLAGS="${CFLAGS} -fPIE" LDFLAGS="${LDFLAGS} -pie" PREFIX="%{prefix}" make

%install
cd %{srcdir}
PREFIX=%{prefix} DESTDIR=%{buildroot} make install

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
%{mandir}/%{name}.1.gz
%{mandir}/%{name}2.1.gz

%changelog

