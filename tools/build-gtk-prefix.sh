#!/bin/sh
# build-gtk-prefix.sh — build a modern GTK4 (plus the deps the system
# is too old for) into a private prefix, leaving the system GTK
# untouched.  Use it for developing/testing Abinova against a newer
# GTK than the distro ships.
#
# Everything lands under $PREFIX (default ~/.local/abinova-gtk-dev):
# sources, build dirs, installed files and the meson/ninja toolchain.
# Nothing is installed system-wide and no sudo is needed.
#
# After it finishes:
#
#   . ~/.local/abinova-gtk-dev/env.sh
#   mkdir build-gtkdev && cd build-gtkdev
#   ../configure --prefix=/tmp/abinova-gtkdev-install
#   make -j$(nproc)
#
# env.sh puts the prefix's pkg-config dir first, so configure picks
# up the new GTK while every other dependency (libgsf, enchant,
# hunspell, boost, …) still comes from the system.
#
# Usage:
#   tools/build-gtk-prefix.sh [--prefix DIR] [--gtk-ref REF]
#                             [--jobs N] [--skip-check]
#
#   --gtk-ref   git ref of gtk.git to build (default: gtk-4-18 branch,
#               i.e. latest 4.18 point release; e.g. use "gtk-4-20"
#               or "master" for newer/development GTK)
#   --skip-check  rebuild every dep even if pkg-config already
#                 satisfies it (e.g. after a git pull)

set -e

prefix="$HOME/.local/abinova-gtk-dev"
gtk_ref="gtk-4-18"
jobs=$(nproc 2>/dev/null || echo 4)
skip_check=0

while [ $# -gt 0 ]; do
	case "$1" in
	--prefix)      prefix=$2; shift 2 ;;
	--prefix=*)    prefix=${1#*=}; shift ;;
	--gtk-ref)     gtk_ref=$2; shift 2 ;;
	--gtk-ref=*)   gtk_ref=${1#*=}; shift ;;
	--jobs|-j)     jobs=$2; shift 2 ;;
	--jobs=*|-j=*) jobs=${1#*=}; shift ;;
	--skip-check)  skip_check=1; shift ;;
	-h|--help)     sed -n '2,30p' "$0"; exit 0 ;;
	*) echo "build-gtk-prefix: unknown option $1" >&2; exit 1 ;;
	esac
done

top=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
toolsdir="$prefix/tools"
srcdir="$prefix/src"
mkdir -p "$prefix" "$toolsdir" "$srcdir"

need_cmd() {
	command -v "$1" >/dev/null 2>&1 || {
		echo "build-gtk-prefix: '$1' not found — please install it" >&2
		exit 1
	}
}
need_cmd git
need_cmd pkg-config
need_cmd gcc
need_cmd g++

# ---------------------------------------------------------- toolchain
# meson + ninja without touching the system (no pip, no apt):
#   ninja  - single-file static binary from GitHub releases
#   meson  - release tarball, runs in place via meson.py

NINJA_VER=1.12.1
MESON_VER=1.6.1

ninja_bin="$toolsdir/ninja"
if ! command -v ninja >/dev/null 2>&1 && [ ! -x "$ninja_bin" ]; then
	echo ">> fetching ninja $NINJA_VER (static binary)"
	curl -fsSL -o "$toolsdir/ninja-linux.zip" \
		"https://github.com/ninja-build/ninja/releases/download/v${NINJA_VER}/ninja-linux.zip"
	python3 -m zipfile -e "$toolsdir/ninja-linux.zip" "$toolsdir/ninja-bin"
	mv "$toolsdir/ninja-bin/ninja" "$ninja_bin"
	chmod +x "$ninja_bin"
	rm -rf "$toolsdir/ninja-bin" "$toolsdir/ninja-linux.zip"
fi
command -v ninja >/dev/null 2>&1 || PATH="$toolsdir:$PATH"

meson_dir="$toolsdir/meson-$MESON_VER"
if ! command -v meson >/dev/null 2>&1 && [ ! -f "$meson_dir/meson.py" ]; then
	echo ">> fetching meson $MESON_VER"
	curl -fsSL -o "$toolsdir/meson.tar.gz" \
		"https://github.com/mesonbuild/meson/releases/download/${MESON_VER}/meson-${MESON_VER}.tar.gz"
	tar -xzf "$toolsdir/meson.tar.gz" -C "$toolsdir"
	rm -f "$toolsdir/meson.tar.gz"
fi
if command -v meson >/dev/null 2>&1; then
	MESON=meson
else
	MESON="python3 $meson_dir/meson.py"
fi

# --------------------------------------------------------- env early
# make prefix installs visible to pkg-config while building deps
libdir_a="$prefix/lib"
libdir_b="$prefix/lib64"
multiarch="$prefix/lib/$(gcc -dumpmachine 2>/dev/null || echo x86_64-linux-gnu)"
export PATH="$prefix/bin:$PATH"
export PKG_CONFIG_PATH="$multiarch/pkgconfig:$libdir_b/pkgconfig:$libdir_a/pkgconfig:$prefix/share/pkgconfig:$PKG_CONFIG_PATH"
export LD_LIBRARY_PATH="$multiarch:$libdir_b:$libdir_a:$LD_LIBRARY_PATH"

have() {  # pkg-config check against prefix+system
	[ $skip_check -eq 0 ] && pkg-config --atleast-version "$2" "$1" 2>/dev/null
}

# ------------------------------------------------------------- build
# build_dep name pkgconfig-name min-version git-url git-ref meson-opts
build_dep() {
	name=$1; mod=$2; minv=$3; repo=$4; ref=$5; shift 5
	if have "$mod" "$minv"; then
		echo "== $name: system/prefix $mod >= $minv already present, skipping"
		return
	fi
	echo "== $name: building $ref -> $prefix"
	if [ ! -d "$srcdir/$name/.git" ]; then
		git clone --depth 1 --branch "$ref" "$repo" "$srcdir/$name" || {
			echo "   branch '$ref' not found, cloning full repo tag/branch name may differ" >&2
			git clone "$repo" "$srcdir/$name"
			git -C "$srcdir/$name" checkout "$ref"
		}
	else
		git -C "$srcdir/$name" fetch --depth 1 origin "$ref" 2>/dev/null || true
		git -C "$srcdir/$name" checkout "$ref" 2>/dev/null || true
		git -C "$srcdir/$name" pull --ff-only 2>/dev/null || true
	fi
	if [ -f "$srcdir/$name/build/meson-private/coredata.dat" ]; then
		$MESON setup --reconfigure "$srcdir/$name/build" "$srcdir/$name" \
			--prefix="$prefix" --libdir=lib \
			--buildtype=release -Ddefault_library=shared "$@"
	else
		rm -rf "$srcdir/$name/build"
		$MESON setup "$srcdir/$name/build" "$srcdir/$name" \
			--prefix="$prefix" --libdir=lib \
			--buildtype=release -Ddefault_library=shared "$@"
	fi
	$MESON compile -C "$srcdir/$name/build" -j "$jobs"
	$MESON install -C "$srcdir/$name/build"
}

# --- glib: only built when the system glib is older than GTK needs.
# gtk-4-18 wants glib >= 2.80 (system has it); newer GTK refs may want
# newer — bump with:  GLIB_MINVER=2.82.0 GLIB_REF=glib-2-82
glib_minver=${GLIB_MINVER:-2.80.0}
glib_ref=${GLIB_REF:-glib-2-80}
build_dep glib glib-2.0 "$glib_minver" \
	https://gitlab.gnome.org/GNOME/glib.git "$glib_ref" \
	-Dtests=false -Dintrospection=disabled -Dman-pages=disabled \
	-Ddocumentation=false -Dnls=disabled -Dlibmount=disabled \
	-Dselinux=disabled

# --- harfbuzz: pango 1.56+ wants >= 8.4; system has 8.3
build_dep harfbuzz harfbuzz 8.4.0 \
	https://github.com/harfbuzz/harfbuzz.git 8.5.0 \
	-Dtests=disabled -Ddocs=disabled -Dintrospection=disabled \
	-Dbenchmark=disabled -Dutilities=disabled

# --- pango: gtk 4.18+ wants >= 1.56; system has 1.52
build_dep pango pango 1.56.0 \
	https://gitlab.gnome.org/GNOME/pango.git 1.56.4 \
	-Dintrospection=disabled -Dbuild-testsuite=false \
	-Dbuild-examples=false -Ddocumentation=false

# --- libepoxy: missing on this system, needed for GTK's GL paths
build_dep epoxy libepoxy 1.5.0 \
	https://github.com/anholt/libepoxy.git 1.5.10 \
	-Dtests=false -Ddocs=false -Degl=yes -Dx11=true -Dglx=yes

# --- libdrm: needed by GTK's Wayland backend, missing on this system
build_dep libdrm libdrm 2.4.99 \
	https://gitlab.freedesktop.org/mesa/drm.git libdrm-2.4.125 \
	-Dintel=disabled -Damdgpu=disabled -Dradeon=disabled -Dnouveau=disabled \
	-Dfreedreno=disabled -Dvc4=disabled -Detnaviv=disabled -Dtegra=disabled \
	-Domap=disabled -Dvmwgfx=disabled -Dcairo-tests=disabled \
	-Dman-pages=disabled -Dvalgrind=disabled -Dinstall-test-programs=false \
	-Dtests=false

# --- gtk itself
build_dep gtk gtk4 4.18.0 \
	https://gitlab.gnome.org/GNOME/gtk.git "$gtk_ref" \
	-Dintrospection=disabled -Ddocumentation=false \
	-Dman-pages=false -Dbuild-demos=false -Dbuild-examples=false \
	-Dbuild-tests=false -Dbuild-testsuite=false \
	-Dmedia-gstreamer=disabled -Dvulkan=disabled \
	-Dcloudproviders=disabled -Dsysprof=disabled -Dtracker=disabled \
	-Dcolord=disabled -Daccesskit=disabled -Dprint-cups=disabled \
	-Dx11-backend=true -Dwayland-backend=true -Dbroadway-backend=false \
	-Dmacos-backend=false -Dwin32-backend=false

# --------------------------------------------------------- env file
envfile="$prefix/env.sh"
cat > "$envfile" <<EOF
# generated by tools/build-gtk-prefix.sh — source before building
# Abinova against the sandboxed GTK in $prefix
export GTKDEV_PREFIX="$prefix"
export PATH="$prefix/bin:\$PATH"
export PKG_CONFIG_PATH="$multiarch/pkgconfig:$libdir_b/pkgconfig:$libdir_a/pkgconfig:$prefix/share/pkgconfig:\$PKG_CONFIG_PATH"
export LD_LIBRARY_PATH="$multiarch:$libdir_b:$libdir_a:\$LD_LIBRARY_PATH"
export GI_TYPELIB_PATH="$multiarch/girepository-1.0:$libdir_a/girepository-1.0:\$GI_TYPELIB_PATH"
export XDG_DATA_DIRS="$prefix/share:\$XDG_DATA_DIRS"
export GSETTINGS_SCHEMA_DIR="$prefix/share/glib-2.0/schemas"
EOF
chmod +x "$envfile"

cat <<EOF

================================================================
GTK dev prefix ready: $prefix
================================================================
  gtk pkg-config version in prefix :
  $(PKG_CONFIG_PATH="$multiarch/pkgconfig:$libdir_b/pkgconfig:$libdir_a/pkgconfig" pkg-config --modversion gtk4 2>/dev/null || echo 'see log above')

To build Abinova against it:

  . "$envfile"
  mkdir -p "$top/build-gtkdev" && cd "$top/build-gtkdev"
  ../configure --disable-maintainer-mode
  make -j$jobs
  ABINOVA_DATADIR="$top" ./src/abinova

Nothing in this prefix affects the system GTK or normal builds —
delete "$prefix" to remove it entirely.
================================================================
EOF
