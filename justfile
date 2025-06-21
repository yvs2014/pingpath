
alias build := build-meson
alias clean := clean-meson
#alias install := install-meson
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
#install-meson: build-meson
#	meson install -C _build
clean-meson:
	rm -rf _build

##
## (2)
## cmake
##
build-cmake:
	test -d _build || cmake -B _build -S $(pwd)
	cmake --build _build
#install-cmake: build-cmake
#	cmake --install _build
clean-cmake:
	rm -rf _build


##
## (3) last resort
## make
##
build-make:
	make
#install-make: build-make
#	make install
clean-make:
	make clean

