:

set -e

TMPL="templates"
cd "$TMPL/alpine"
abuild -rc
cd -
ls -lR ~/packages/"$TMPL"

