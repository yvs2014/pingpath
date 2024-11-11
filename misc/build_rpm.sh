:

set -e

SRCDIR="$(cd $(dirname $0)/..; pwd)"
RPMDIR="$SRCDIR/rpms"

cd "$SRCDIR/templates/rpmlike"
rpmbuild -ba --define "_sourcedir $SRCDIR" --define "_rpmdir $RPMDIR" pingpath.spec || :
cd ..

#ls -lR "$RPM"
tree "$RPM"

