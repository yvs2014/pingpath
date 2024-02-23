:

set -e
RPM=rpms
mkdir -p "$RPM"
RPMDEST="$(pwd)/$RPM"

cd rpmlinux
rpmbuild -ba --define "_rpmdir $RPMDEST" pingpath.spec || :
cd ..

#ls -lR "$RPM"
tree "$RPM"

