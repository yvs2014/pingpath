:

set -e
AUR=aur
export PKGDEST="$(pwd)/$AUR"

mkdir -p "$AUR"
cd archlinux
makepkg -cf || :
cd -
ls -l "$AUR"

