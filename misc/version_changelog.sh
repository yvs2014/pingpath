:

set -e
LANG=C

NAME='pingpath'
VERPRFX='0.2'
TAG='b215bec'

BACKUP=
MD_CHANGELOG='CHANGELOG.md'
DEB_CHANGELOG='debian/changelog'

FILE='common.h'
PATT='#define\sVERSION\s'
FILE_SNAP='snap/snapcraft.yaml'
PATT_SNAP='version:\s'

[ $# -lt 1 ] && { echo "Use: $(basename $0) 'string with comment'"; exit 1; }

git_comments=
md_comments=
deb_comments=
for m in "$@"; do
	git_comments="$git_comments -m \"$m\""
	md_comments="$md_comments\n- $m"
	deb_comments="$deb_comments\n  * $m\n"
done


vers="$(git rev-list --count $TAG...HEAD)"
next=$(($vers + 1))
vn="$VERPRFX.$next"
[ -n "$BACKUP" ] && cp "$FILE" "/tmp/$(basename $FILE).bk"
sed -i "s/^\($PATT\).*/\1\"$vn\"/" "$FILE"
[ -n "$BACKUP" ] && cp "$FILE_SNAP" "/tmp/$(basename $FILE_SNAP).bk"
sed -i "s/^\($PATT_SNAP\).*/\1$vn/" "$FILE_SNAP"

## md format
[ -n "$BACKUP" ] && cp "$MD_CHANGELOG" "/tmp/$(basename $MD_CHANGELOG).bk"
_mf="/tmp/$(basename $MD_CHANGELOG).merge"
printf "## $VERPRFX.${next}$md_comments\n\n" | cat - "$MD_CHANGELOG" >"$_mf"
mv "$_mf" "$MD_CHANGELOG"

## deb format
[ -n "$BACKUP" ] && cp "$DEB_CHANGELOG" "/tmp/$(basename $DEB_CHANGELOG).bk"
_mntr=$(sed -n 's/^Maintainer: //p' debian/control)
_tf="/tmp/$(basename $DEB_CHANGELOG).$$"
cp "$DEB_CHANGELOG" "$_tf"
printf "$NAME ($vn) mantic noble; urgency=low\n$deb_comments\n -- $_mntr  $(date -R)\n\n" | cat - "$_tf" >"$DEB_CHANGELOG"
rm -f "$_tf"

echo "Keep in mind:"
echo "	git diff"
echo "	git status"
echo "	make clean && make && make clean"
echo "	git add ."
echo "	git commit $git_comments"
echo "	git push"

