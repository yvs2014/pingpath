
alias build := build-meson
alias clean := clean-meson
alias install := install-meson
alias test := test-meson
#
alias meson := build-meson
alias cmake := build-cmake
alias make  := build-make

##
## (1) preferable
## meson
##
build-meson:
	test -d _build || meson setup _build
	meson compile -C _build
test-meson: build-meson
	meson test -C _build
install-meson: build-meson
	meson install -C _build
clean-meson:
	rm -rf _build

##
## (2)
## cmake
##
build-cmake:
	test -d _build || cmake -B _build -S $(pwd)
	cmake --build _build
install-cmake: build-cmake
	cmake --install _build
clean-cmake:
	rm -rf _build


##
## (3) last resort
## make
##
build-make:
	make
install-make: build-make
	make install
clean-make:
	make clean

##
## packaging
##
TMPL := "templates"
alias debian := deb
alias alp    := apk
alias alpine := apk
alias arch   := aur

check_dep dep hint:
	@command -v {{dep}} >/dev/null || (echo ">>> '{{dep}}' is not found (hint: {{hint}})"; exit 1;)

deb: \
(check_dep "dpkg-buildpackage" "apt install dpkg-dev" ) \
(check_dep "dh"                "apt install debhelper") \
(check_dep "pkg-config"        "apt install pkgconf"  )
	@mkdir -p debs
	DEBDIR="--destdir=debs"                \
	dpkg-buildpackage -b -tc --no-sign     \
	--buildinfo-option="-udebs"            \
	--buildinfo-file="debs/last.buildinfo" \
	--changes-option="-udebs"              \
	--changes-file="debs/last.changes"
	@ls -lR debs

rpm: (check_dep "rpmbuild" "dnf install rpm-build")
	@cd "{{TMPL}}/rpmlike" && \
	rpmbuild -ba --define "_sourcedir ../.." --define "_rpmdir ../../rpms" pingpath.spec
	@ls -lR rpms

aur: (check_dep "makepkg" "it needs 'makepkg'")
	@mkdir -p "aur"
	@cd "{{TMPL}}/aurlike" && \
	makepkg -cf
	@cd -
	@ls -l aur

apk: (check_dep "abuild" "apk add abuild")
	@cd "{{TMPL}}/alpine" && \
	abuild -rc
	@cd -
	@ls -lR ~/packages/"{{TMPL}}"

snap: (check_dep "snapcraft" "snap install snapcraft")
	snapcraft
	@ls -l *.snap

#flat:
#	flatpak-builder --user --install build-dir flatpak.yaml --force-clean

