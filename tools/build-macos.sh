#!/bin/sh
# build-macos.sh — set up dependencies and build Abinova on macOS.
#
# Abinova's UI is pure GTK4, so on macOS it runs on GTK's Quartz
# backend — there is no separate Cocoa port.  Everything this script
# installs comes from Homebrew; no X11 is required (GTK/Quartz
# provides its own windowing, and the source tree guards all
# remaining Xlib calls behind GDK_WINDOWING_X11).
#
# Usage:
#   tools/build-macos.sh [--prefix DIR] [--jobs N] [--skip-deps]
#
# Tested against: macOS 13+ (arm64 & x86_64), Homebrew GTK 4.x.

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
		sed -n '2,16p' "$0"; exit 0 ;;
	*) echo "build-macos: unknown option $1" >&2; exit 1 ;;
	esac
done

top=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

if ! command -v brew >/dev/null 2>&1; then
	echo "build-macos: Homebrew not found." >&2
	echo "Install it from https://brew.sh, then re-run." >&2
	exit 1
fi

brew_prefix=$(brew --prefix)

# ------------------------------------------------------------ deps
if [ $skip_deps -eq 0 ]; then
	brew install \
		autoconf automake libtool pkg-config \
		gtk4 cairo pango librsvg fribidi \
		libgsf libxslt zlib libpng jpeg-turbo \
		enchant hunspell \
		boost \
		perl make
	# libtool installs as glibtool/glibtoolize on macOS; make sure
	# autoreconf can find them
fi

# pkg-config and friends need the brewed prefix in front, plus
# keg-only packages that macOS already provides older copies of
export PATH="$brew_prefix/bin:$brew_prefix/opt/gettext/bin:$brew_prefix/opt/libtool/libexec/gnubin:$PATH"
export PKG_CONFIG_PATH="$brew_prefix/lib/pkgconfig:$brew_prefix/share/pkgconfig:$PKG_CONFIG_PATH"
export CPPFLAGS="-I$brew_prefix/include $CPPFLAGS"
export LDFLAGS="-L$brew_prefix/lib $LDFLAGS"
# jpeg-turbo and libxslt are keg-only
export PKG_CONFIG_PATH="$brew_prefix/opt/jpeg-turbo/lib/pkgconfig:$brew_prefix/opt/libxslt/lib/pkgconfig:$PKG_CONFIG_PATH"

# -------------------------------------------------------- configure
cfg="$top/configure"
if [ ! -f "$cfg" ] || [ "$top/configure.ac" -nt "$cfg" ]; then
	( cd "$top" && autoreconf -f -i )
fi

args="--disable-maintainer-mode"
[ -n "$prefix" ] && args="$args --prefix=$prefix"

mkdir -p "$top/build-macos"
cd "$top/build-macos"

# shellcheck disable=SC2086
"$cfg" $args "$@"

# ------------------------------------------------------------ build
gmake -j"$jobs" || make -j"$jobs"

cat <<EOF

Abinova built in $top/build-macos
Run from the build tree with:

  ABINOVA_DATADIR="$top" $PWD/src/abinova

or install with:  (cd $top/build-macos && gmake install)

EOF
