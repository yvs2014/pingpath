:

set -e
AUR=aur
PKGDEST="$(pwd)/$AUR"
export PKGDEST

mkdir -p "$AUR"
cd templates/aurlike
makepkg -cf || :
cd -
ls -l "$AUR"

