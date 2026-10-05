#!/bin/sh
# build-linux.sh — set up dependencies and build Abinova on Linux.
#
# Abinova's UI is pure GTK4; on Linux it runs on GDK's Wayland or X11
# backend with no extra port work needed.  The only distro-specific
# part is the dependency-install step: this script knows two package
# maps — apt (Debian/Ubuntu and derivatives) and zypper
# (openSUSE/SLES) — picked by --distro or autodetected from
# /etc/os-release (ID first, then ID_LIKE so derivatives like
# Zorin/Mint/Pop!_OS resolve to their upstream family's map).
#
# Usage:
#   tools/build-linux.sh [--distro debian|ubuntu|suse]
#                        [--prefix DIR] [--jobs N]
#                        [--skip-deps] [--print-deps]
#
# apt/zypper need root — the deps step escalates via sudo/doas when
# not already root; --skip-deps avoids it entirely, and --print-deps
# just prints the resolved package list for the detected (or
# --distro-selected) distro and exits — useful when you cannot sudo.
#
# Hunspell itself is vendored (thirdparty/hunspell-1.7.4); the distro
# hunspell package is installed for the CLI/tools and an en_US
# dictionary is installed best-effort because the grammar checker
# loads dictionaries from the standard dirs (PORT04).
#
# Tested against: Debian 12+/Ubuntu 24.04+ (apt), openSUSE Leap and
# Tumbleweed (zypper).  Zorin OS resolves through ID_LIKE=ubuntu.

set -e

prefix=""
jobs=$(nproc 2>/dev/null || echo 4)
skip_deps=0
print_deps=0
distro=""

while [ $# -gt 0 ]; do
	case "$1" in
	--distro)    distro=$2; shift 2 ;;
	--distro=*)  distro=${1#*=}; shift ;;
	--prefix)    prefix=$2; shift 2 ;;
	--prefix=*)  prefix=${1#*=}; shift ;;
	--jobs|-j)   jobs=$2; shift 2 ;;
	--jobs=*|-j=*) jobs=${1#*=}; shift ;;
	--skip-deps) skip_deps=1; shift ;;
	--print-deps) print_deps=1; shift ;;
	-h|--help)
		sed -n '2,30p' "$0"; exit 0 ;;
	*) echo "build-linux: unknown option $1" >&2; exit 1 ;;
	esac
done

top=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

if [ "$(uname -s)" != "Linux" ]; then
	echo "build-linux: this script targets Linux (uname: $(uname -s))" >&2
	exit 1
fi

# ----------------------------------------------------- distro detect
# Map a list of os-release id tokens onto a package-manager family.
family_for_ids() {
	for id in $1; do
		case "$id" in
		debian|ubuntu)                          echo deb;  return 0 ;;
		suse|opensuse|opensuse-*|sles|sled)     echo suse; return 0 ;;
		esac
	done
	return 1
}

detect_src=
if [ -n "$distro" ]; then
	family=$(family_for_ids "$distro") || {
		echo "build-linux: unknown distro '$distro'" >&2
		echo "(valid: debian, ubuntu, suse)" >&2
		exit 1
	}
	detect_src="--distro $distro"
else
	osr_id=
	osr_like=
	if [ -r /etc/os-release ]; then
		# os-release is a shell-compatible KEY=VALUE file; extract
		# just the two fields (values may be quoted, e.g.
		# ID_LIKE="ubuntu debian")
		osr_id=$(sed -n 's/^ID=//p' /etc/os-release | head -n1 | tr -d '"')
		osr_like=$(sed -n 's/^ID_LIKE=//p' /etc/os-release | head -n1 | tr -d '"')
	fi
	family=$(family_for_ids "$osr_id") && detect_src="os-release ID=$osr_id" || {
		family=$(family_for_ids "$osr_like") && \
			detect_src="os-release ID_LIKE (ID=$osr_id)" || {
			echo "build-linux: cannot identify distro" >&2
			echo "(os-release ID='${osr_id:-?}' ID_LIKE='${osr_like:-?}')" >&2
			echo "supported families: debian/ubuntu (apt), suse (zypper)" >&2
			echo "re-run with --distro debian|ubuntu|suse, or install" >&2
			echo "the deps manually and re-run with --skip-deps" >&2
			exit 1
		}
	}
fi

# ------------------------------------------------------------ deps
# Package maps cover everything configure.ac requires:
#   toolchain  (gcc/g++ for C++17, make, perl, autotools, pkg-config)
#   gtk4 >= 4.14.5 + gtk4-unix-print, cairo, pango/pangocairo, librsvg
#   fribidi, glib/gio, libgsf, libxslt(+libxml2), zlib
#   libpng, libjpeg, libtiff (image loaders), libX11 (X11 backend)
#   boost headers >= 1.83, enchant-2 (spell), hunspell dict data
# Names differ per family (-dev vs -devel).
deb_deps="build-essential autoconf automake libtool pkg-config
	libgtk-4-dev libcairo2-dev libpango1.0-dev librsvg2-dev
	libfribidi-dev libgsf-1-dev libxslt1-dev libxml2-dev
	zlib1g-dev libpng-dev libjpeg-dev libtiff-dev
	libglib2.0-dev libx11-dev
	libenchant-2-dev hunspell
	libboost-dev
	perl"
# en_US dictionary — separate so a missing name can't sink the
# whole install; other languages are hunspell-<lang>
deb_dict_pkg=hunspell-en-us

suse_deps="gcc gcc-c++ make autoconf automake libtool pkg-config
	gtk4-devel cairo-devel pango-devel librsvg-devel
	fribidi-devel libgsf-devel libxslt-devel libxml2-devel
	zlib-devel libpng16-devel libjpeg8-devel libtiff-devel
	glib2-devel libX11-devel
	enchant-devel hunspell
	boost-devel
	perl"
suse_dict_pkg=myspell-en_US

case "$family" in
deb)  deps=$deb_deps;  dict_pkg=$deb_dict_pkg ;;
suse) deps=$suse_deps; dict_pkg=$suse_dict_pkg ;;
esac

if [ $print_deps -eq 1 ]; then
	echo "build-linux: distro family '$family' (detected via $detect_src)"
	echo "packages:"
	# one per line, indented
	echo "$deps" | tr -s ' \t\n' '\n' | sed 's/^/  /'
	echo "  $dict_pkg (best-effort dictionary)"
	exit 0
fi

if [ $skip_deps -eq 0 ]; then
	SU=
	if [ "$(id -u)" -ne 0 ]; then
		if command -v doas >/dev/null 2>&1; then
			SU=doas
		elif command -v sudo >/dev/null 2>&1; then
			SU=sudo
		else
			echo "build-linux: dependency install needs root (or install" >&2
			echo "sudo/doas, or re-run with --skip-deps after installing" >&2
			echo "the packages — see --print-deps)" >&2
			exit 1
		fi
	fi
	case "$family" in
	deb)
		if ! command -v apt-get >/dev/null 2>&1; then
			echo "build-linux: apt-get not found — is this a Debian/Ubuntu system?" >&2
			exit 1
		fi
		# shellcheck disable=SC2086
		$SU apt-get update
		# shellcheck disable=SC2086
		$SU apt-get install -y $deps
		# dictionary is best-effort (name could drift on derivatives)
		$SU apt-get install -y "$dict_pkg" || true
		;;
	suse)
		if ! command -v zypper >/dev/null 2>&1; then
			echo "build-linux: zypper not found — is this an openSUSE/SLES system?" >&2
			exit 1
		fi
		# shellcheck disable=SC2086
		$SU zypper --non-interactive install $deps
		$SU zypper --non-interactive install "$dict_pkg" || true
		;;
	esac
fi

# -------------------------------------------------------- configure
cfg="$top/configure"
if [ ! -f "$cfg" ] || [ "$top/configure.ac" -nt "$cfg" ]; then
	( cd "$top" && autoreconf -f -i )
fi

args="--disable-maintainer-mode"
[ -n "$prefix" ] && args="$args --prefix=$prefix"

mkdir -p "$top/build-linux"
cd "$top/build-linux"

# shellcheck disable=SC2086
"$cfg" $args "$@"

# ------------------------------------------------------------ build
make -j"$jobs"

cat <<EOF

Abinova built in $top/build-linux
Run from the build tree with:

  ABINOVA_DATADIR="$top" $PWD/src/abinova

or install with:  (cd $top/build-linux && make install)

EOF
