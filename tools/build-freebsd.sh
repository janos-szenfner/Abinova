#!/bin/sh
# build-freebsd.sh — set up dependencies and build Abinova on FreeBSD.
#
# Abinova's UI is pure GTK4, which ports/pkg provides — GTK's X11 and
# Wayland backends both work on FreeBSD and no X11-specific port work
# is needed (the remaining Xlib calls in the tree are compiled only
# when GDK_WINDOWING_X11 is set, which the gtk4 package satisfies).
#
# FreeBSD notes baked in below:
#  * "make" is BSD make; automake output requires GNU make -> gmake.
#  * ports/pkg install under /usr/local, which the base clang does NOT
#    search by default — CPPFLAGS/LDFLAGS and PKG_CONFIG_PATH are set
#    accordingly (and libdata/pkgconfig is a FreeBSD-specific .pc dir
#    glib and friends use for arch-independent modules).
#  * zlib and iconv live in the base system, no package needed.
#
# Usage:
#   tools/build-freebsd.sh [--prefix DIR] [--jobs N] [--skip-deps]
#
# pkg install needs root — the deps step runs under doas/sudo when not
# already root; --skip-deps avoids it entirely.
#
# Authored on Linux against the ports tree — needs a FreeBSD runner
# for the first real build check.
#
# Tested against: FreeBSD 14+ (amd64/arm64).

set -e

prefix=""
jobs=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
skip_deps=0

while [ $# -gt 0 ]; do
	case "$1" in
	--prefix)    prefix=$2; shift 2 ;;
	--prefix=*)  prefix=${1#*=}; shift ;;
	--jobs|-j)   jobs=$2; shift 2 ;;
	--jobs=*|-j=*) jobs=${1#*=}; shift ;;
	--skip-deps) skip_deps=1; shift ;;
	-h|--help)
		sed -n '2,27p' "$0"; exit 0 ;;
	*) echo "build-freebsd: unknown option $1" >&2; exit 1 ;;
	esac
done

top=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

if [ "$(uname -s)" != "FreeBSD" ]; then
	echo "build-freebsd: this script targets FreeBSD (uname: $(uname -s))" >&2
	exit 1
fi

if ! command -v pkg >/dev/null 2>&1; then
	echo "build-freebsd: pkg not found — is this a real FreeBSD system?" >&2
	exit 1
fi

# ------------------------------------------------------------ deps
if [ $skip_deps -eq 0 ]; then
	SU=
	if [ "$(id -u)" -ne 0 ]; then
		if command -v doas >/dev/null 2>&1; then
			SU=doas
		elif command -v sudo >/dev/null 2>&1; then
			SU=sudo
		else
			echo "build-freebsd: pkg install needs root (or install doas/sudo," >&2
			echo "or re-run with --skip-deps after installing the deps)" >&2
			exit 1
		fi
	fi
	$SU pkg install -y \
		autoconf automake libtool pkgconf gmake perl5 \
		gtk4 cairo pango fribidi \
		libgsf libxslt libxml2 \
		glib png jpeg-turbo \
		enchant2 hunspell \
		boost-libs
	# librsvg is librsvg2-rust on current ports trees; older ones
	# called it librsvg2 — try both names
	$SU pkg install -y librsvg2-rust || $SU pkg install -y librsvg2
fi

# ports/pkg install under /usr/local — neither the base clang nor
# pkgconf is guaranteed to look there (pkgconf covers it, but being
# explicit costs nothing and also catches AC_CHECK_HEADER checks for
# png.h/jpeglib.h that don't go through pkg-config)
export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:/usr/local/libdata/pkgconfig:$PKG_CONFIG_PATH"
export CPPFLAGS="-I/usr/local/include $CPPFLAGS"
export LDFLAGS="-L/usr/local/lib $LDFLAGS"

# -------------------------------------------------------- configure
cfg="$top/configure"
if [ ! -f "$cfg" ] || [ "$top/configure.ac" -nt "$cfg" ]; then
	( cd "$top" && autoreconf -f -i )
fi

args="--disable-maintainer-mode"
[ -n "$prefix" ] && args="$args --prefix=$prefix"

mkdir -p "$top/build-freebsd"
cd "$top/build-freebsd"

# shellcheck disable=SC2086
"$cfg" $args "$@"

# ------------------------------------------------------------ build
gmake -j"$jobs"

cat <<EOF

Abinova built in $top/build-freebsd
Run from the build tree with:

  ABINOVA_DATADIR="$top" $PWD/src/abinova

or install with:  (cd $top/build-freebsd && gmake install)

EOF
