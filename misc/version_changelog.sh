:

set -e
LANG=C

NAME='pingpath'
VERPRFX='0.3'
TAG='d3da356'

BACKUP=
MD_CHANGELOG='CHANGELOG.md'
DEB_CHANGELOG='debian/changelog'

FILE='common.h'
PATT='#define\sVERSION\s'

[ $# -lt 1 ] && { echo "Use: $(basename "$0") 'string with comment'"; exit 1; }

git_comments=
md_comments=
deb_comments=
for m in "$@"; do
	git_comments="$git_comments -m \"$m\""
	md_comments="$md_comments\n- $m"
	deb_comments="$deb_comments\n  * $m\n"
done


vers="$(git rev-list --count $TAG..HEAD)"
next=$((vers + 1))
vn="$VERPRFX.$next"
[ -n "$BACKUP" ] && cp "$FILE" "/tmp/$(basename $FILE).bk"
sed -i "s/^\($PATT\).*/\1\"$vn\"/" "$FILE"

## md format
[ -n "$BACKUP" ] && cp "$MD_CHANGELOG" "/tmp/$(basename $MD_CHANGELOG).bk"
_mf="/tmp/$(basename $MD_CHANGELOG).merge"
printf "## %s.%s%s\n\n" "$VERPRFX" "$next" "$md_comments" | cat - "$MD_CHANGELOG" >"$_mf"
mv "$_mf" "$MD_CHANGELOG"

## deb format
[ -n "$BACKUP" ] && cp "$DEB_CHANGELOG" "/tmp/$(basename $DEB_CHANGELOG).bk"
_mntr=$(sed -n 's/^Maintainer: //p' debian/control)
_tf="/tmp/$(basename $DEB_CHANGELOG).$$"
cp "$DEB_CHANGELOG" "$_tf"
printf "%s (%s) plucky; urgency=low\n%s\n -- %s  %s\n\n" "$NAME" "$vn" "$deb_comments" "$_mntr" "$(date -R)"| cat - "$_tf" >"$DEB_CHANGELOG"
rm -f "$_tf"

echo "Keep in mind:"
echo "	git diff"
echo "	git status"
echo "	just && just clean"
echo "	git add ."
echo "	git commit $git_comments"
echo "	git push"

