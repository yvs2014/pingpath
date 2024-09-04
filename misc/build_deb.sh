:

APPNAME='pingpath'
BASEVER='0.3'
GTAG='d3da356'
DDIR="debs"

# build depends on: dpkg-dev debhelper
chk_cmd() {
  command -v "$1" >/dev/null && return
  echo "FAIL: '$1' is mandatory for packaging, please install '$2'"
  exit 1
}
chk_cmd dpkg-buildpackage dpkg-dev
chk_cmd dh debhelper
chk_cmd pkg-config "pkgconf (or pkg-config on some derives)"

set -e
command -v git >/dev/null && rev="$(git rev-list --count $GTAG..HEAD)" || rev=
[ -n "$rev" ] && vers="$BASEVER.$rev" || vers="$BASEVER"
arch="$(dpkg-architecture -qDEB_BUILD_ARCH)"
nra="$DDIR/${APPNAME}_${vers}_$arch"

mkdir -p "$DDIR"

bi_file="$nra.buildinfo"
ch_file="$nra.changes"
dpkg-buildpackage --help | grep -q buildinfo-file && \
  BOUT="--buildinfo-file=$bi_file" COUT="--changes-file=$ch_file" || \
  BOUT="--buildinfo-option=-O$bi_file" COUT="--changes-option=-O$ch_file" DH_OPTIONS="--destdir=$DDIR"

export DEBDIR="--destdir=$DDIR"
dpkg-buildpackage -b -tc --no-sign \
  --buildinfo-option="-u$DDIR" $BOUT \
  --changes-option="-u$DDIR" $COUT && \
  (echo "\nPackages in $DDIR/:"; ls -l "$DDIR")

