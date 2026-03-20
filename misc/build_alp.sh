:

set -e

TMPL="templates"
cd "$TMPL/alpine"
abuild -c
cd -
ls -lR ~/packages/"$TMPL"

