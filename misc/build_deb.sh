:

name='pingpath'
basever='0.1'

# build depends on: dpkg-dev debhelper
chk_cmd() {
  command -v "$1" >/dev/null && return
  echo "FAIL: '$1' is mandatory for packaging, please install '$2'"
  exit 1
}
chk_cmd dpkg-buildpackage dpkg-dev
chk_cmd dh debhelper

set -e
command -v git >/dev/null && rev="$(git rev-list HEAD | sed -n '$=')" || rev=
[ -n "$rev" ] && vers="$basever.$rev" || vers="$basever"
arch="$(dpkg-architecture -qDEB_BUILD_ARCH)"
ddir="debs"
nra="$ddir/${name}_${vers}_$arch"

mkdir -p "$ddir"

bi_file="$nra.buildinfo"
ch_file="$nra.changes"
dpkg-buildpackage --help | grep -q buildinfo-file && \
  BOUT="--buildinfo-file=$bi_file" COUT="--changes-file=$ch_file" || \
  BOUT="--buildinfo-option=-O$bi_file" COUT="--changes-option=-O$ch_file" DH_OPTIONS="--destdir=$ddir"

export DEBDIR="--destdir=$ddir"
dpkg-buildpackage -b -tc --no-sign \
  --buildinfo-option="-u$ddir" $BOUT \
  --changes-option="-u$ddir" $COUT && \
  (echo "\nPackages in $ddir/:"; ls -l "$ddir")

