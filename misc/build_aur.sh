:

set -e
AUR=aur
export PKGDEST="$(pwd)/$AUR"

mkdir -p "$AUR"
cd templates/aurlike
makepkg -cf || :
cd -
ls -l "$AUR"

