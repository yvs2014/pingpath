:

set -e
LANG=C

NAME='pingpath'
VERPRFX='0.1'

BACKUP=
MD_CHANGELOG='CHANGELOG.md'
DEB_CHANGELOG='debian/changelog'

FILE='common.h'
PATT='#define\sVERSION\s'

[ $# -lt 1 ] && { echo "Use: $(basename $0) 'string with comment'"; exit 1; }

git_comments=
md_comments=
deb_comments=
for m in "$@"; do
	git_comments="$git_comments -m \"$m\""
	md_comments="$md_comments\n- $m"
	deb_comments="$deb_comments\n  * $m\n"
done


[ -n "$TAG0" ] && { fltr=sed; fargs="/^$TAG0/q"; } || fltr=cat

vers="$(git rev-list HEAD | $fltr $fargs | sed -n '$=')"
next=$(($vers + 1))
vn="$VERPRFX.$next"
[ -n "$BACKUP" ] && cp "$FILE" "/tmp/$(basename $FILE).bk"
sed -i "s/^\($PATT\).*/\1\"$vn\"/" "$FILE"

## md format
[ -n "$BACKUP" ] && cp "$MD_CHANGELOG" "/tmp/$(basename $MD_CHANGELOG).bk"
printf "## $VERPRFX.${next}$md_comments\n" > "$MD_CHANGELOG"
git log | while read w r; do
	if [ "$w" = 'commit' ]; then
		printf "\n## $VERPRFX.$vers\n"
		vers=$(($vers - 1))
	elif [ -z "$w" -o "$w" = 'Author:' -o "$w" = 'Date:' ]; then
		:
	else
		echo "- $w $r"
	fi
done >>"$MD_CHANGELOG"

## deb format
[ -n "$BACKUP" ] && cp "$DEB_CHANGELOG" "/tmp/$(basename $DEB_CHANGELOG).bk"
_mntr=$(sed -n 's/^Maintainer: //p' debian/control)
_tf="/tmp/$(basename $DEB_CHANGELOG).$$"
trap "rm -f $_tf" EXIT TERM
cp "$DEB_CHANGELOG" "$_tf"
printf "$NAME ($vn) lunar mantic; urgency=low\n$deb_comments\n -- $_mntr  $(date -R)\n\n" | cat - "$_tf" >"$DEB_CHANGELOG"

echo "Keep in mind:"
echo "	git diff"
echo "	git status"
echo "	make clean && make && make clean"
echo "	git add ."
echo "	git commit $git_comments"
echo "	git push"

