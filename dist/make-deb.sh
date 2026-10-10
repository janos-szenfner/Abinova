#!/bin/sh
# make-deb.sh — hand-roll a .deb wrapping the self-contained Linux
# bundle produced by dist/linux-bundle.sh.
#
# The bundle is relocatable, so the package simply installs it verbatim
# under /opt/abinova-<ver>/ and wires system integration directly (no
# postinst dependency on install-desktop.sh):
#
#   /opt/abinova-<ver>/...                whole bundle (RUNPATH $ORIGIN/../lib)
#   /usr/bin/abinova                      symlink -> /opt/abinova-<ver>/bin/abinova
#   /usr/share/applications/*.desktop     Exec rewritten to /usr/bin/abinova
#   /usr/share/icons/hicolor/...          app icons
#   /usr/share/metainfo/*.xml             AppStream metainfo
#   /usr/share/mime/packages/abinova.xml  .abwn MIME registration
#
# postinst/postrm refresh the desktop/mime/icon caches when the host
# provides the tools (all calls guarded — none is a hard dependency).
# The package declares no Depends: the bundle carries its full library
# closure (dist/BUNDLED-DEPS.md) and only needs a libc-family system.
#
# Usage:
#   dist/make-deb.sh [--bundle DIR] [--out DIR] [--release N] [-h|--help]
#
#   --bundle    staged bundle dir (default: dist/abinova-<ver>-linux-<arch>)
#   --out       where to write the .deb (default: dist/)
#   --release   Debian revision suffix (default: 1) -> <ver>-<release>
#
# Output: dist/abinova_<ver>-<rel>_<debarch>.deb
# Requires: dpkg-deb (with --root-owner-group; dpkg >= 1.19).
# Tested on: Linux x86-64.

set -eu

bundle=""
outdir=""
release=1

while [ $# -gt 0 ]; do
	case "$1" in
	--bundle)     bundle=$2; shift 2 ;;
	--bundle=*)   bundle=${1#*=}; shift ;;
	--out)        outdir=$2; shift 2 ;;
	--out=*)      outdir=${1#*=}; shift ;;
	--release)    release=$2; shift 2 ;;
	--release=*)  release=${1#*=}; shift ;;
	-h|--help)    sed -n '2,38p' "$0"; exit 0 ;;
	*) echo "make-deb: unknown option $1" >&2; exit 1 ;;
	esac
done

command -v dpkg-deb >/dev/null 2>&1 || {
	echo "make-deb: dpkg-deb not found" >&2; exit 1; }

top=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

uname_m=$(uname -m)
case $uname_m in
x86_64|amd64)  debarch=amd64 ;;
aarch64|arm64) debarch=arm64 ;;
*)             debarch=$uname_m ;;
esac

vmaj=$(sed -n 's/^m4_define(\[abi_version_major\], \[\(.*\)\])/\1/p' "$top/configure.ac")
vmin=$(sed -n 's/^m4_define(\[abi_version_minor\], \[\(.*\)\])/\1/p' "$top/configure.ac")
vmic=$(sed -n 's/^m4_define(\[abi_version_micro\], \[\(.*\)\])/\1/p' "$top/configure.ac")
version=$vmaj.$vmin.$vmic

[ -n "$bundle" ] || bundle="$top/dist/abinova-$version-linux-$uname_m"
[ -n "$outdir" ] || outdir="$top/dist"

[ -x "$bundle/bin/abinova" ] || {
	echo "make-deb: no $bundle/bin/abinova — run dist/linux-bundle.sh first" >&2
	exit 1; }

prefix=/opt/abinova-$version
stage=$(mktemp -d /tmp/abinova-deb.XXXXXX)
trap 'rm -rf "$stage"' EXIT

# --- payload --------------------------------------------------------------
mkdir -p "$stage$prefix"
cp -a "$bundle/." "$stage$prefix/"

mkdir -p "$stage/usr/bin"
ln -s "$prefix/bin/abinova" "$stage/usr/bin/abinova"

# desktop integration — same assets install-desktop.sh wires, landed as
# tracked package files so dpkg removes them again on purge
mkdir -p "$stage/usr/share/applications"
for f in "$bundle"/share/applications/*.desktop; do
	[ -f "$f" ] || continue
	sed "s|^Exec=.*|Exec=/usr/bin/abinova %U|" "$f" \
		> "$stage/usr/share/applications/$(basename "$f")"
done
for d in icons/hicolor metainfo mime/packages; do
	[ -d "$bundle/share/$d" ] || continue
	mkdir -p "$stage/usr/share/$(dirname "$d")"
	cp -a "$bundle/share/$d" "$stage/usr/share/$(dirname "$d")/"
done

mkdir -p "$stage/usr/share/doc/abinova"
{
	echo "Abinova $version — word processor (self-contained Linux bundle)."
	echo "Bundled third-party components and their licences:"
	echo "  $prefix/BUNDLE-INFO.txt (manifest)"
	echo "  dist/THIRD-PARTY-NOTICES.md in the source tree (notices)"
} > "$stage/usr/share/doc/abinova/copyright"

# --- control --------------------------------------------------------------
mkdir -p "$stage/DEBIAN"
installed_size=$(du -sk "$stage" | cut -f1)
cat > "$stage/DEBIAN/control" <<EOF
Package: abinova
Version: $version-$release
Architecture: $debarch
Maintainer: Abinova developers <https://github.com/janos-szenfner/Abinova>
Installed-Size: $installed_size
Section: editors
Priority: optional
Homepage: https://github.com/janos-szenfner/Abinova
Description: Abinova word processor (self-contained bundle)
 GTK4 word processor forked from AbiWord: DOCX/DOC/ODT/RTF/EPUB
 import, abwn native format. This package ships the self-contained
 dist/linux-bundle.sh payload under /opt — it carries its own library
 closure and has no package dependencies beyond libc.
EOF

cat > "$stage/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
command -v update-desktop-database >/dev/null 2>&1 &&
	update-desktop-database /usr/share/applications || true
command -v update-mime-database >/dev/null 2>&1 &&
	update-mime-database /usr/share/mime || true
command -v gtk-update-icon-cache >/dev/null 2>&1 &&
	gtk-update-icon-cache -q /usr/share/icons/hicolor || true
exit 0
EOF
cp "$stage/DEBIAN/postinst" "$stage/DEBIAN/postrm"
chmod 755 "$stage/DEBIAN/postinst" "$stage/DEBIAN/postrm"

deb="$outdir/abinova_$version-${release}_$debarch.deb"
dpkg-deb --build --root-owner-group "$stage" "$deb"
echo "make-deb: wrote $deb"
