:

set -e
AUR=aur
export PREFIX="/usr"
export PKGDEST="$(pwd)/$AUR"

mkdir -p "$AUR"
cd archlinux
#makepkg -cf || updpkgsums && makepkg -cf || :
makepkg -cf || :
cd ..
ls -l "$AUR"

