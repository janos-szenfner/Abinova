#!/bin/sh
# build-windows-msys2.sh — set up dependencies and build Abinova for
# Windows inside an MSYS2 (MINGW64/UCRT64) shell.
#
# Abinova's UI is pure GTK4, so on Windows it runs on GDK's native
# Win32 backend — there is no separate Win32 port and no X11/Xlib
# dependency (all remaining X11 calls in the tree are guarded behind
# GDK_WINDOWING_X11, which is off for win32 GDK builds).
#
# Run inside an MSYS2 "UCRT64" shell (the one with the light-blue
# prompt), e.g.:
#
#   cd /c/work/exp-abi
#   tools/build-windows-msys2.sh
#
# Usage: build-windows-msys2.sh [--prefix DIR] [--jobs N] [--skip-deps]

set -e

prefix=""
jobs=$(nproc 2>/dev/null || echo 4)
skip_deps=0

while [ $# -gt 0 ]; do
	case "$1" in
	--prefix)    prefix=$2; shift 2 ;;
	--prefix=*)  prefix=${1#*=}; shift ;;
	--jobs|-j)   jobs=$2; shift 2 ;;
	--jobs=*|-j=*) jobs=${1#*=}; shift ;;
	--skip-deps) skip_deps=1; shift ;;
	-h|--help)
		sed -n '2,18p' "$0"; exit 0 ;;
	*) echo "build-windows: unknown option $1" >&2; exit 1 ;;
	esac
done

top=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

case "${MSYSTEM:-}" in
	MINGW64|UCRT64|CLANG64) ;;
	*)
		echo "build-windows: run this inside an MSYS2 MINGW64/UCRT64 shell" >&2
		echo "(MSYSTEM is '${MSYSTEM:-unset}', expected MINGW64 or UCRT64)" >&2
		exit 1 ;;
esac

# pick the right pacman package prefix for the active environment
# (mingw-w64-ucrt-x86_64-* on UCRT64, mingw-w64-x86_64-* on MINGW64)
pkgpfx="${MINGW_PACKAGE_PREFIX}"

# ------------------------------------------------------------ deps
if [ $skip_deps -eq 0 ]; then
	pacman -Sy --needed --noconfirm \
		base-devel \
		"${pkgpfx}-toolchain" \
		"${pkgpfx}-autotools" \
		autoconf automake-wrapper libtool pkgconf \
		"${pkgpfx}-gtk4" \
		"${pkgpfx}-cairo" \
		"${pkgpfx}-pango" \
		"${pkgpfx}-librsvg" \
		"${pkgpfx}-fribidi" \
		"${pkgpfx}-libgsf" \
		"${pkgpfx}-libxslt" \
		"${pkgpfx}-zlib" \
		"${pkgpfx}-libpng" \
		"${pkgpfx}-libjpeg-turbo" \
		"${pkgpfx}-enchant" \
		"${pkgpfx}-hunspell" \
		"${pkgpfx}-boost" \
		"${pkgpfx}-glib2-devel" \
		perl make
fi

# -------------------------------------------------------- configure
cfg="$top/configure"
if [ ! -f "$cfg" ] || [ "$top/configure.ac" -nt "$cfg" ]; then
	( cd "$top" && autoreconf -f -i )
fi

args="--disable-maintainer-mode"
# no dbus/accounts integration on Windows
args="$args --without-darwinports"
[ -n "$prefix" ] && args="$args --prefix=$prefix"

mkdir -p "$top/build-windows"
cd "$top/build-windows"

# shellcheck disable=SC2086
"$cfg" $args "$@"

# ------------------------------------------------------------ build
make -j"$jobs"

cat <<EOF

Abinova built in $top/build-windows
Run it from the build tree with:

  ABINOVA_DATADIR="$(cygpath -w "$top" 2>/dev/null || echo "$top")" $PWD/src/abinova.exe

or install with:  (cd $top/build-windows && make install)

Note: the produced .exe needs the MSYS2 runtime DLLs on its PATH —
either launch it from the MSYS2 shell, or copy the GTK stack with
'ldd src/abinova.exe' as a guide when packaging for distribution.
EOF
