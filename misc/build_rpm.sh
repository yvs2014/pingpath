:

set -e

#SRCDIR="$(cd "$(dirname "$0")/.."; pwd)"
#RPMDIR="$SRCDIR/rpms"

cd "templates/rpmlike"
rpmbuild -ba --define "_sourcedir ../.." --define "_rpmdir ../../rpms" pingpath.spec || :
cd ..

#ls -lR rpms"
tree rpms

