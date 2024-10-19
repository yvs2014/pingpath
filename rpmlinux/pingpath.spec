
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
BuildRequires: meson, git, pkgconf, gtk4-devel, json-glib-devel, libglvnd-devel, libepoxy-devel, cglm-devel
BuildRequires: (gcc or clang)

%description
Network diagnostic tool based on parsing ping output with some functionality of traceroute.
Written using GTK4 framework.

%define srcdir %{name}
%define prefix /usr
%define datdir %{prefix}/share
%define dskdir %{datdir}/applications
%define docdir %{_docdir}/%{name}

%prep
rm -rf %{srcdir}
git clone https://github.com/yvs2014/%{name}

%build
cd %{srcdir}
%meson
%meson_build

%install
cd %{srcdir}
%meson_install

%check
cd %{srcdir}
%meson_test

%files
%defattr(-,root,root,-)
%dir %{docdir}
%dir %{docdir}/examples
%{_bindir}/%{name}
%{_bindir}/%{name}2
%{dskdir}/net.tools.%{name}.desktop
%{dskdir}/net.tools.%{name}2.desktop
%{datdir}/icons/hicolor/scalable/apps/%{name}.svg
%{docdir}/examples/%{name}.conf
%{_mandir}/man1/%{name}.1*

%changelog
# autofill

