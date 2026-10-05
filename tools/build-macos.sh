#!/bin/sh
# build-macos.sh — set up dependencies, build Abinova on macOS, and
# optionally assemble a self-contained Abinova.app.
#
# Abinova's UI is pure GTK4, so on macOS it runs on GTK's Quartz
# backend — there is no separate Cocoa port.  Everything this script
# installs comes from Homebrew; no X11 is required (GTK/Quartz
# provides its own windowing, and the source tree guards all
# remaining Xlib calls behind GDK_WINDOWING_X11).
#
# Usage:
#   tools/build-macos.sh [--prefix DIR] [--jobs N] [--skip-deps]
#                        [--bundle[=DIR]] [--universal] [--no-sign]
#                        [--bundle-only] [--no-verify]
#
# Bundle stage (--bundle):
#   Stages 'make install' and assembles a relocatable .app:
#
#     Abinova.app/Contents/
#       Info.plist                          icon + UTIs (.abwn/.abw
#                                           exported, docx/doc/odt/rtf
#                                           public UTIs)
#       MacOS/abinova                       launcher: pins
#                                           ABINOVA_DATADIR,
#                                           ABINOVA_MODULE_ROOT,
#                                           GDK_BACKEND=quartz,
#                                           GDK_PIXBUF_MODULE_FILE,
#                                           GSETTINGS_SCHEMA_DIR,
#                                           execs abinova-bin
#       MacOS/abinova-bin                   the real binary
#       Frameworks/*.dylib                  brew dep closure; every
#                                           install name rewritten to
#                                           @executable_path/../Frameworks/<name>
#                                           (dylibbundler semantics via
#                                           manual otool/install_name_tool —
#                                           no dylibbundler dep needed)
#       Frameworks/gdk-pixbuf-2.0/loaders/  pixbuf loaders + loaders.cache
#                                           (cache paths relative — the app
#                                           rewrites them absolute at startup)
#       Frameworks/gio/modules/             GIO modules incl. the libgiognutls
#                                           TLS backend + giomodule.cache
#       Frameworks/enchant-2/               spellcheck backends (self-loaded)
#       Frameworks/gtk-4.0/<ver>/           immodules/media/printbackends
#       Frameworks/gstreamer-1.0/           media plugins
#       Resources/                          datadir contents (artwork/,
#                                           fonts/, help/, ...)
#       Resources/hunspell|hyphen/          PACK07 dictionaries
#       Resources/fontconfig/               private fonts.conf + conf.d
#       Resources/glib-2.0/schemas/         gschemas.compiled
#       Resources/certs/ca-certificates.crt CA store for the update check
#       Resources/abinova.icns              bundle icon
#
#   The verify pass re-walks every Mach-O file in the bundle and fails
#   if any dep resolves outside Contents/Frameworks other than the
#   declared macOS system set (/usr/lib, /System).  GTK4's Quartz
#   backend is compiled into libgtk-4 itself (brew builds
#   -Dmacos-backend=true), so bundling the lib covers it — the
#   launcher pins GDK_BACKEND=quartz as a belt-and-braces default.
#   The dlopen'd runtime modules (enchant backends, GIO modules incl.
#   the libgiognutls TLS backend + CA bundle, GTK4 module dirs,
#   GStreamer plugins) are staged by the PACK06 section below.
#
#   Apple Silicon notes:
#   * arm64 Mach-O refuses to launch unsigned, so the bundle stage
#     ad-hoc codesigns every dylib/binary and the .app itself
#     (codesign -s -).  Real Developer-ID signing + notarization is
#     PACK09.
#   * --universal produces a universal2 app on Apple Silicon: it needs
#     Rosetta 2 plus a second (x86_64) Homebrew at /usr/local —
#     i.e. 'arch -x86_64 /usr/local/bin/brew' installed.  Both archs
#     are built and staged, then lipo-merged per Mach-O file; a dep
#     present in only one brew is copied thin with a warning.
#
# Tested against: macOS 13+ (arm64 & x86_64), Homebrew GTK 4.x.

set -e

prefix=""
jobs=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
skip_deps=0
skip_build=0
bundle=0
appdir=""
universal=0
sign=1
verify=1

while [ $# -gt 0 ]; do
	case "$1" in
	--prefix)    prefix=$2; shift 2 ;;
	--prefix=*)  prefix=${1#*=}; shift ;;
	--jobs|-j)   jobs=$2; shift 2 ;;
	--jobs=*|-j=*) jobs=${1#*=}; shift ;;
	--skip-deps) skip_deps=1; shift ;;
	--bundle)    bundle=1; shift ;;
	--bundle=*)  bundle=1; appdir=${1#*=}; shift ;;
	--universal) universal=1; shift ;;
	--no-sign)   sign=0; shift ;;
	--bundle-only) bundle=1; skip_deps=1; skip_build=1; shift ;;
	--no-verify) verify=0; shift ;;
	-h|--help)
		sed -n '2,65p' "$0"; exit 0 ;;
	*) echo "build-macos: unknown option $1" >&2; exit 1 ;;
	esac
done

top=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

if [ "$(uname -s)" != "Darwin" ]; then
	echo "build-macos: this script targets macOS (uname: $(uname -s))" >&2
	exit 1
fi

if ! command -v brew >/dev/null 2>&1; then
	echo "build-macos: Homebrew not found." >&2
	echo "Install it from https://brew.sh, then re-run." >&2
	exit 1
fi

brew_prefix=$(brew --prefix)
host_arch=$(uname -m)

# --universal only works one direction: an arm64 host can build the
# x86_64 slice under Rosetta against a second brew at /usr/local.
# (The reverse — fattening on Intel — has no arm64 brew or toolchain.)
other_arch=
otherbrew=
if [ $universal -eq 1 ]; then
	if [ "$host_arch" != "arm64" ]; then
		echo "build-macos: --universal requires an Apple Silicon host" >&2
		echo "  (Rosetta + an x86_64 brew at /usr/local); on Intel" >&2
		echo "  the build is x86_64-only anyway." >&2
		exit 1
	fi
	other_arch=x86_64
	otherbrew=/usr/local
	if ! arch -x86_64 /usr/bin/true 2>/dev/null; then
		echo "build-macos: --universal needs Rosetta 2:" >&2
		echo "  softwareupdate --install-rosetta --agree-to-license" >&2
		exit 1
	fi
	if [ ! -x "$otherbrew/bin/brew" ]; then
		echo "build-macos: --universal needs an x86_64 Homebrew at" >&2
		echo "  /usr/local (install with: arch -x86_64 /bin/bash -c" >&2
		echo "  \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\"" >&2
		echo "  then: arch -x86_64 /usr/local/bin/brew install <deps>)" >&2
		exit 1
	fi
fi

# ------------------------------------------------------------ deps
if [ $skip_deps -eq 0 ]; then
	brew install \
		autoconf automake libtool pkg-config \
		gtk4 cairo pango librsvg fribidi \
		libgsf libxslt zlib libpng jpeg-turbo \
		enchant hunspell \
		glib-networking ca-certificates gstreamer \
		boost \
		perl make
	if [ $universal -eq 1 ]; then
		arch -x86_64 "$otherbrew/bin/brew" install \
			autoconf automake libtool pkg-config \
			gtk4 cairo pango librsvg fribidi \
			libgsf libxslt zlib libpng jpeg-turbo \
			enchant hunspell \
			glib-networking ca-certificates gstreamer \
			boost \
			perl make
	fi
	# libtool installs as glibtool/glibtoolize on macOS; make sure
	# autoreconf can find them
fi

# -------------------------------------------------------- configure
cfg="$top/configure"
if [ ! -f "$cfg" ] || [ "$top/configure.ac" -nt "$cfg" ]; then
	( cd "$top" && autoreconf -f -i )
fi

args="--disable-maintainer-mode"
[ -n "$prefix" ] && args="$args --prefix=$prefix"

# env_for <brew-prefix> — must be called inside a subshell/function
# scope that owns the environment (universal builds juggle two).
env_for() {
	export PATH="$1/bin:$1/opt/gettext/bin:$1/opt/libtool/libexec/gnubin:$PATH"
	export PKG_CONFIG_PATH="$1/lib/pkgconfig:$1/share/pkgconfig:$1/opt/jpeg-turbo/lib/pkgconfig:$1/opt/libxslt/lib/pkgconfig"
	export CPPFLAGS="-I$1/include ${CPPFLAGS:-}"
	export LDFLAGS="-L$1/lib ${LDFLAGS:-}"
}

build_in() { # $1 = build dir; $2 = brew prefix; $3.. = runner prefix (e.g. arch -x86_64)
	bdir=$1
	bp=$2
	shift 2
	mkdir -p "$bdir"
	(
		env_for "$bp"
		cd "$bdir"
		# shellcheck disable=SC2086
		"$@" "$cfg" $args
		"$@" gmake -j"$jobs" || "$@" make -j"$jobs"
	)
}

bdir1="$top/build-macos"
bdir2="$top/build-macos-$other_arch"

if [ $skip_build -eq 0 ]; then
	build_in "$bdir1" "$brew_prefix"
	[ $universal -eq 1 ] && build_in "$bdir2" "$otherbrew" arch -x86_64
fi

if [ $bundle -eq 0 ]; then
	cat <<EOF

Abinova built in $bdir1
Run from the build tree with:

  ABINOVA_DATADIR="$top" $bdir1/src/abinova

or install with:  (cd $bdir1 && gmake install)
or bundle with:   $0 --bundle-only --skip-deps

EOF
	exit 0
fi

# ==================================================== bundle stage
echo "build-macos: assembling .app bundle"

for t in otool install_name_tool; do
	command -v "$t" >/dev/null 2>&1 || {
		echo "build-macos: $t not found — install Xcode CLT" >&2
		echo "  (xcode-select --install)" >&2
		exit 1; }
done
if [ $universal -eq 1 ] && ! command -v lipo >/dev/null 2>&1; then
	echo "build-macos: --universal needs lipo (Xcode CLT)" >&2
	exit 1
fi

vmaj=$(sed -n 's/^m4_define(\[abi_version_major\], \[\(.*\)\])/\1/p' "$top/configure.ac")
vmin=$(sed -n 's/^m4_define(\[abi_version_minor\], \[\(.*\)\])/\1/p' "$top/configure.ac")
vmic=$(sed -n 's/^m4_define(\[abi_version_micro\], \[\(.*\)\])/\1/p' "$top/configure.ac")
version=$vmaj.$vmin.$vmic
series=$vmaj.$vmin

[ -n "$appdir" ] || appdir="$top/dist/Abinova.app"

# --------------------------------------------------- staged install
stage1="$bdir1/stage"
stage2="$bdir2/stage"
if [ $skip_build -eq 0 ] || [ ! -f "$stage1.installed" ]; then
	rm -rf "$stage1"
	mkdir -p "$bdir1"
	echo "build-macos: staging 'make install' into $stage1"
	make -C "$bdir1" install DESTDIR="$stage1" > "$bdir1/install.log" 2>&1 || {
		tail -n 20 "$bdir1/install.log" >&2
		echo "build-macos: make install failed (log: $bdir1/install.log)" >&2
		exit 1; }
	: > "$stage1.installed"
fi
if [ $universal -eq 1 ]; then
	rm -rf "$stage2"
	make -C "$bdir2" install DESTDIR="$stage2" > "$bdir2/install.log" 2>&1 || {
		tail -n 20 "$bdir2/install.log" >&2
		echo "build-macos: x86_64 make install failed" >&2
		exit 1; }
fi

pref1=$(dirname "$(dirname "$(find "$stage1" -type f -name abinova \
	-path '*/bin/*' | head -n1)")")
if [ ! -f "$pref1/bin/abinova" ]; then
	echo "build-macos: no abinova binary under $stage1" >&2
	exit 1
fi
if [ $universal -eq 1 ]; then
	pref2=$(dirname "$(dirname "$(find "$stage2" -type f -name abinova \
		-path '*/bin/*' | head -n1)")")
	[ -f "$pref2/bin/abinova" ] || {
		echo "build-macos: no x86_64 abinova binary under $stage2" >&2
		exit 1; }
fi
datasrc="$pref1/share/abinova-$series"
[ -d "$datasrc" ] || {
	echo "build-macos: datadir $datasrc missing from stage" >&2; exit 1; }

is_macho() {
	case "$(od -An -tx1 -N4 "$1" 2>/dev/null | tr -d ' \n')" in
	cafebabe|cafebabf|bebafeca|bebafecb|cffaedfe|cecfaedf|feedfacf|feedface)
		return 0 ;;
	esac
	return 1
}

is_system_dep() {
	case "$1" in
	/usr/lib/*|/System/*) return 0 ;;
	esac
	return 1
}

# Bookkeeping that must survive pipelines lives in temp files — a
# `cmd | while read` loop runs in a subshell, so variable updates
# inside it are lost.  Created early: the pixbuf/dep-walk passes
# below all append to these.
walklist=$(mktemp "${TMPDIR:-/tmp}/abinova-walk.XXXXXX")
depsfile=$(mktemp "${TMPDIR:-/tmp}/abinova-deps.XXXXXX")
thinfile=$(mktemp "${TMPDIR:-/tmp}/abinova-thin.XXXXXX")
warnfile=$(mktemp "${TMPDIR:-/tmp}/abinova-warn.XXXXXX")
failfile=$(mktemp "${TMPDIR:-/tmp}/abinova-fail.XXXXXX")
copiedmark=$(mktemp "${TMPDIR:-/tmp}/abinova-copied.XXXXXX")
rm -f "$copiedmark" "$failfile"
trap 'rm -f "$walklist" "$depsfile" "$thinfile" "$warnfile" "$failfile" "$copiedmark"' EXIT

# -------------------------------------------- .app skeleton + plist
rm -rf "$appdir"
macos="$appdir/Contents/MacOS"
res="$appdir/Contents/Resources"
fw="$appdir/Contents/Frameworks"
mkdir -p "$macos" "$res" "$fw"

# stage_file <rel-under-prefix> <dest> — copy, or lipo-merge when a
# same-arch counterpart exists in the secondary stage.
stage_file() {
	s1="$pref1/$1"
	if [ $universal -eq 1 ] && [ -f "$pref2/$1" ] \
		&& is_macho "$s1" && is_macho "$pref2/$1"; then
		lipo -create "$s1" "$pref2/$1" -output "$2"
	else
		cp -aL "$s1" "$2"
	fi
}

stage_file bin/abinova "$macos/abinova-bin"
chmod 755 "$macos/abinova-bin"
is_macho "$macos/abinova-bin" || {
	echo "build-macos: staged bin/abinova is not a Mach-O binary" >&2
	echo "  (libtool wrapper? 'make install' should produce a real one)" >&2
	exit 1; }

for lib in "$pref1"/lib/libabinova-*.dylib; do
	[ -f "$lib" ] || continue
	base=${lib##*/}
	rel="lib/$base"
	if [ $universal -eq 1 ] && [ -f "$pref2/$rel" ]; then
		lipo -create "$lib" "$pref2/$rel" -output "$fw/$base"
	else
		cp -aL "$lib" "$fw/$base"
	fi
done

# datadir contents -> Resources (launcher exports ABINOVA_DATADIR=Resources)
cp -a "$datasrc/." "$res/"

# bundle icon
cp "$top/icons/abinova.icns" "$res/abinova.icns"

# ------------------------------------------------- gdk-pixbuf loaders
pixdir=$(find "$brew_prefix/lib" -type d \
	-path '*gdk-pixbuf-2.0/*/loaders' 2>/dev/null | sort | tail -n1)
if [ -n "$pixdir" ]; then
	bload="$fw/gdk-pixbuf-2.0/loaders"
	mkdir -p "$bload"
	for so in "$pixdir"/*.so; do
		base=${so##*/}
		if [ $universal -eq 1 ]; then
			so2=$(echo "$so" | sed "s|^$brew_prefix|$otherbrew|")
			if [ -f "$so2" ]; then
				lipo -create "$so" "$so2" -output "$bload/$base" || \
					cp -aL "$so" "$bload/$base"
			else
				cp -aL "$so" "$bload/$base"
				echo "$so" >> "$thinfile"
			fi
		else
			cp -aL "$so" "$bload/$base"
		fi
	done
	query=$(command -v gdk-pixbuf-query-loaders 2>/dev/null || true)
	[ -n "$query" ] || query=$(find "$brew_prefix" -type f \
		-name 'gdk-pixbuf-query-loaders*' 2>/dev/null | head -n1)
	if [ -n "$query" ]; then
		# module paths relative to the cache's own dir — relocatable
		"$query" "$bload"/*.so 2>/dev/null | \
			sed "s|\"$bload/|\"loaders/|g" \
			> "$fw/gdk-pixbuf-2.0/loaders.cache"
	else
		echo "build-macos: WARNING no gdk-pixbuf-query-loaders —" >&2
		echo "  loaders copied but no loaders.cache generated" >&2
	fi
else
	echo "build-macos: WARNING no gdk-pixbuf loaders dir found" >&2
fi

# ------------------------------------------------ gschemas.compiled
schema_dir="$res/glib-2.0/schemas"
mkdir -p "$schema_dir"
schema_src=""
for d in "$brew_prefix/share/glib-2.0/schemas" \
	"$brew_prefix"/opt/*/share/glib-2.0/schemas; do
	[ -d "$d" ] && cp -a "$d"/*.xml "$schema_dir/" 2>/dev/null && schema_src=1
done
if [ -n "$schema_src" ] && \
	command -v glib-compile-schemas >/dev/null 2>&1; then
	glib-compile-schemas --strict --targetdir="$schema_dir" \
		"$schema_dir" 2>/dev/null || \
		glib-compile-schemas --targetdir="$schema_dir" "$schema_dir"
	rm -f "$schema_dir"/*.xml
else
	echo "build-macos: WARNING glib schemas/compiler missing" >&2
fi

# ---------------------------------------- PACK06 runtime modules
# dlopen'd modules — invisible to otool -L of the binary, so they are
# staged explicitly; the closure walk below picks up THEIR deps.
# The launcher exports ABINOVA_MODULE_ROOT=Frameworks and the app's
# runtime-module setup (xap_UnixApp::_setBundleModulePaths) maps the
# GIO/GTK/GStreamer dirs plus the enchant backends under it.
bundle_modules() { # $1 = brew srcdir, $2 = dest dir under $fw
	src=$1
	dst="$fw/$2"
	[ -d "$src" ] || return 1
	mkdir -p "$dst"
	for so in "$src"/*.so; do
		[ -f "$so" ] || continue
		base=${so##*/}
		if [ $universal -eq 1 ]; then
			so2=$(echo "$so" | sed "s|^$brew_prefix|$otherbrew|")
			if [ -f "$so2" ]; then
				lipo -create "$so" "$so2" -output "$dst/$base" || \
					cp -aL "$so" "$dst/$base"
			else
				cp -aL "$so" "$dst/$base"
				echo "$so" >> "$thinfile"
			fi
		else
			cp -aL "$so" "$dst/$base"
		fi
	done
	return 0
}

# GIO modules — libgiognutls is the TLS backend the update check needs
if bundle_modules "$brew_prefix/lib/gio/modules" "gio/modules"; then
	query=$(command -v gio-querymodules 2>/dev/null || true)
	[ -n "$query" ] || query=$(find "$brew_prefix" -type f \
		-name 'gio-querymodules*' 2>/dev/null | head -n1)
	[ -n "$query" ] && "$query" "$fw/gio/modules" 2>/dev/null || true
else
	echo "build-macos: WARNING no gio modules dir (brew glib-networking?)" >&2
fi

# enchant spellcheck backends — the app self-loads
# $ABINOVA_MODULE_ROOT/enchant-2 when the broker's compiled-in dir
# has no providers (brew enchant is not built --enable-relocatable)
bundle_modules "$brew_prefix/lib/enchant-2" "enchant-2" || \
	echo "build-macos: WARNING no enchant backends dir found" >&2

# GTK4 module dirs (immodules/media/printbackends, incl. their
# giomodule.cache — bare module names, already relocatable)
gtkroot=$(find "$brew_prefix/lib" -type d -name printbackends \
	-path '*gtk-4.0*' 2>/dev/null | sort | tail -n1)
if [ -n "$gtkroot" ]; then
	gtkroot=$(dirname "$gtkroot")	# gtk-4.0/<binary-version>
	relgtk=${gtkroot##*/lib/}
	# copy whole subdirs — they carry giomodule.cache files as well as
	# modules — then lipo-merge each .so for the universal case
	for sub in "$gtkroot"/*/; do
		[ -d "$sub" ] || continue
		sub=${sub%/}
		mkdir -p "$fw/$relgtk/${sub##*/}"
		cp -aL "$sub/." "$fw/$relgtk/${sub##*/}/"
		for so in "$sub"/*.so; do
			[ -f "$so" ] || continue
			base=${so##*/}
			if [ $universal -eq 1 ]; then
				so2=$(echo "$so" | sed "s|^$brew_prefix|$otherbrew|")
				if [ -f "$so2" ]; then
					lipo -create "$so" "$so2" \
						-output "$fw/$relgtk/${sub##*/}/$base" || true
				fi
			fi
		done
	done
else
	echo "build-macos: WARNING no gtk-4.0 module dir found" >&2
fi

# GStreamer plugins for GTK4's libmedia-gstreamer.so
bundle_modules "$brew_prefix/lib/gstreamer-1.0" "gstreamer-1.0" || \
	echo "build-macos: WARNING no gstreamer-1.0 plugins dir found" >&2

# -------------------------------------------------- PACK07 data files
# hunspell dictionaries + hyphenation patterns -> Resources/hunspell +
# Resources/hyphen.  The app appends the bundle root to XDG_DATA_DIRS
# and probes <AbiSuiteLibDir>/{hunspell,hyphen} directly, so both the
# enchant providers and the grammar/hyphenation paths find them.
# Brew ships no dictionaries of its own — whatever the host has under
# share/hunspell (a hunspell-* formula or a manual drop) is what lands.
copy_dicts() { # $1 = dest dir, $2 = name filter; rest = src dirs (first wins)
	dictdst=$1; filt=$2; shift 2
	mkdir -p "$dictdst"
	for src in "$@"; do
		[ -d "$src" ] || continue
		for f in "$src"/*.aff "$src"/*.dic; do
			[ -f "$f" ] || continue
			base=${f##*/}
			case "$filt:$base" in
			spell:hyph_*.dic) ;;	# patterns belong to the hyphen dir
			hyph:hyph_*.dic)
				[ -f "$dictdst/$base" ] || cp -aL "$f" "$dictdst/$base" ;;
			hyph:*) ;;
			*) [ -f "$dictdst/$base" ] || cp -aL "$f" "$dictdst/$base" ;;
			esac
		done
	done
}
copy_dicts "$res/hunspell" spell \
	"$brew_prefix/share/hunspell" /Library/Spelling ~/Library/Spelling
copy_dicts "$res/hyphen" hyph \
	"$brew_prefix/share/hyphen" "$brew_prefix/share/hunspell" \
	/Library/Spelling ~/Library/Spelling
[ -f "$res/hunspell/en_US.dic" ] || [ -f "$res/hunspell/en_GB.dic" ] || \
	echo "build-macos: WARNING no English hunspell dictionary staged —" >&2
	echo "  install one on the build host (e.g. drop en_US.aff/.dic" >&2
	echo "  into $brew_prefix/share/hunspell) for bundled spellcheck" >&2

# fontconfig — macOS has no system fontconfig configuration at all
# (brew's lives under its prefix, outside the bundle), so ship a
# private config the launcher points FONTCONFIG_FILE at.  The bundled
# fonts/ collection itself is registered by the app via
# FcConfigAppFontAddDir; this file only has to supply the system
# font dirs, a writable cache dir and the generic conf.d rules.
mkdir -p "$res/fontconfig"
cat > "$res/fontconfig/fonts.conf" <<'FC'
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "fonts.dtd">
<fontconfig>
	<dir>/System/Library/Fonts</dir>
	<dir>/Library/Fonts</dir>
	<dir>~/Library/Fonts</dir>
	<include ignore_missing="yes">conf.d</include>
	<cachedir>~/Library/Caches/fontconfig</cachedir>
</fontconfig>
FC
if [ -d "$brew_prefix/etc/fonts/conf.d" ]; then
	# cp -aL resolves the conf.d symlinks into real files — their
	# targets in share/fontconfig/conf.avail stay outside otherwise
	cp -aL "$brew_prefix/etc/fonts/conf.d" "$res/fontconfig/"
else
	echo "build-macos: WARNING no brew fontconfig conf.d —" >&2
	echo "  fontconfig runs on the minimal fonts.conf alone" >&2
fi

# CA bundle for the update check — Resources/certs/ca-certificates.crt
# is probed by xap_UpdateCheck.cpp (app-relative; the host trust
# store stays the fallback).
capfx=$(brew --prefix ca-certificates 2>/dev/null || echo "$brew_prefix")
for c in "$capfx/share/ca-certificates/cacert.pem" \
	"$brew_prefix/etc/ca-certificates/cert.pem" \
	"$brew_prefix/etc/openssl@3/cert.pem"; do
	if [ -f "$c" ]; then
		mkdir -p "$res/certs"
		cp -aL "$c" "$res/certs/ca-certificates.crt"
		break
	fi
done
[ -f "$res/certs/ca-certificates.crt" ] || \
	echo "build-macos: WARNING no CA bundle found" >&2

# ------------------------------------------------ dylib closure walk
# Iterate to a fixpoint: every Mach-O already in the bundle feeds its
# dep list; each copied dylib may pull more deps.
dep_list() { # dep refs of $1, one per line, own install name excluded
	ownid=$(otool -D "$1" 2>/dev/null | sed -n '2p')
	otool -L "$1" 2>/dev/null | tail -n +2 | \
		sed 's/^[ 	]*//; s/ .*$//' | while IFS= read -r d; do
		[ "$d" = "$ownid" ] || echo "$d"
	done
}

find_brew_lib() { # basename -> first match under a brew prefix
	find "$brew_prefix/lib" "$brew_prefix/opt" \
		${otherbrew:+"$otherbrew/lib" "$otherbrew/opt"} \
		-name "$1" -type f 2>/dev/null | head -n1
}

copy_dep() { # $1 = dep ref (abs path or @rpath/@loader_path name)
	d=$1
	base=${d##*/}
	[ -f "$fw/$base" ] && return 0
	src=$d
	if [ ! -f "$src" ]; then
		src=$(find_brew_lib "$base")
		[ -n "$src" ] || {
			grep -qxF "$d" "$warnfile" 2>/dev/null || {
				echo "build-macos: WARNING unresolved dep $d" >&2
				echo "$d" >> "$warnfile"; }
			return 0; }
	fi
	if [ $universal -eq 1 ]; then
		# map a brew path to the other arch's brew prefix
		case "$src" in
		"$brew_prefix"/*) d2="$otherbrew/${src#"$brew_prefix"/}" ;;
		"$otherbrew"/*)   d2="$brew_prefix/${src#"$otherbrew"/}" ;;
		*)                d2="" ;;
		esac
		if [ -n "$d2" ] && [ -f "$d2" ]; then
			lipo -create "$src" "$d2" -output "$fw/$base" 2>/dev/null || {
				cp -aL "$src" "$fw/$base"; echo "$src" >> "$thinfile"; }
		else
			cp -aL "$src" "$fw/$base"
			echo "$src" >> "$thinfile"
		fi
	else
		cp -aL "$src" "$fw/$base"
	fi
}

while :; do
	find "$macos" "$fw" -type f > "$walklist"
	while IFS= read -r f; do
		is_macho "$f" || continue
		dep_list "$f" > "$depsfile"
		while IFS= read -r d; do
			is_system_dep "$d" && continue
			case "$d" in
			@executable_path/../Frameworks/*) continue ;;
			esac
			base=${d##*/}
			[ -f "$fw/$base" ] && continue
			case "$d" in
			"$brew_prefix"/*|"${otherbrew:-$brew_prefix}"/*| \
			@rpath/*|@loader_path/*|/*)
				copy_dep "$d"
				[ -f "$fw/$base" ] && : >> "$copiedmark" ;;
			esac
		done < "$depsfile"
	done < "$walklist"
	[ -f "$copiedmark" ] || break
	rm -f "$copiedmark"
done

echo "build-macos: $(find "$fw" -maxdepth 1 -name '*.dylib' | wc -l | tr -d ' ') dylibs in Frameworks/"

# ------------------------------------------------ install-name rewrite
# Every dep ref to a bundled lib -> @executable_path/../Frameworks/<name>;
# bundled dylibs get that as their own -id.  @executable_path resolves
# to Contents/MacOS (the dir of abinova-bin) for every Mach-O in the
# bundle, so one convention covers exes, dylibs and nested modules.
rewrite_refs() { # $1 = mach-o file
	f=$1
	set --
	dep_list "$f" > "$depsfile"
	while IFS= read -r d; do
		is_system_dep "$d" && continue
		base=${d##*/}
		if [ -f "$fw/$base" ]; then
			case "$d" in
			@executable_path/../Frameworks/*) ;;
			*) set -- "$@" -change "$d" \
				"@executable_path/../Frameworks/$base" ;;
			esac
		else
			case "$d" in
			@executable_path/*) ;;
			*) echo "build-macos: WARNING ${f##*/} keeps" \
				"out-of-bundle dep $d" >&2 ;;
			esac
		fi
	done < "$depsfile"
	[ $# -gt 0 ] && install_name_tool "$@" "$f"
	case "$f" in
	*.dylib) install_name_tool -id \
		"@executable_path/../Frameworks/${f##*/}" "$f" ;;
	esac
	# strip rpaths pointing at a brew prefix (dead after relocation)
	otool -l "$f" 2>/dev/null | awk '/LC_RPATH/{r=1} r&&/path /{print $2; r=0}' \
		> "$depsfile"
	while IFS= read -r rp; do
		case "$rp" in
		"$brew_prefix"/*|"${otherbrew:-$brew_prefix}"/*)
			install_name_tool -delete_rpath "$rp" "$f" 2>/dev/null || true ;;
		esac
	done < "$depsfile"
}

find "$macos" "$fw" -type f > "$walklist"
while IFS= read -r f; do
	is_macho "$f" && rewrite_refs "$f"
done < "$walklist"

# ------------------------------------------------------------ launcher
cat > "$macos/abinova" <<'LAUNCHER'
#!/bin/sh
# Abinova.app launcher — points the app at its bundled resources.
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
contents=$(dirname "$here")
res="$contents/Resources"
fw="$contents/Frameworks"

export ABINOVA_DATADIR="$res"
export GDK_BACKEND="${GDK_BACKEND:-quartz}"
# PACK06: tell the app's runtime-module setup where the Frameworks
# module dirs live — it exports GIO_EXTRA_MODULES / GTK_PATH /
# GST_PLUGIN_SYSTEM_PATH_1_0 and fixes GDK_PIXBUF_MODULE_FILE itself.
# enchant backends under $fw/enchant-2 are self-loaded via the
# provider ABI; the CA bundle is probed at $res/certs/.
export ABINOVA_MODULE_ROOT="$fw"
export GDK_PIXBUF_MODULE_FILE="$fw/gdk-pixbuf-2.0/loaders.cache"
export GSETTINGS_SCHEMA_DIR="$res/glib-2.0/schemas"
# PACK07: private fontconfig (macOS has none system-wide) — the app
# probes $res/fontconfig/fonts.conf itself, the export just makes
# the intent explicit and lets a user override win.
[ -f "$res/fontconfig/fonts.conf" ] && \
	export FONTCONFIG_FILE="${FONTCONFIG_FILE:-$res/fontconfig/fonts.conf}"

exec "$here/abinova-bin" "$@"
LAUNCHER
chmod 755 "$macos/abinova"

# ---------------------------------------------------------- Info.plist
cat > "$appdir/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleExecutable</key>
	<string>abinova</string>
	<key>CFBundleIdentifier</key>
	<string>io.github.janos_szenfner.Abinova</string>
	<key>CFBundleName</key>
	<string>Abinova</string>
	<key>CFBundleDisplayName</key>
	<string>Abinova</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleShortVersionString</key>
	<string>$version</string>
	<key>CFBundleVersion</key>
	<string>$version</string>
	<key>CFBundleIconFile</key>
	<string>abinova</string>
	<key>CFBundleInfoDictionaryVersion</key>
	<string>6.0</string>
	<key>LSMinimumSystemVersion</key>
	<string>13.0</string>
	<key>NSHighResolutionCapable</key>
	<true/>
	<key>NSSupportsAutomaticGraphicsSwitching</key>
	<true/>
	<key>CFBundleDocumentTypes</key>
	<array>
		<dict>
			<key>CFBundleTypeName</key>
			<string>Abinova Document</string>
			<key>CFBundleTypeRole</key>
			<string>Editor</string>
			<key>LSHandlerRank</key>
			<string>Owner</string>
			<key>CFBundleTypeIconFile</key>
			<string>abinova</string>
			<key>LSItemContentTypes</key>
			<array>
				<string>io.github.janos_szenfner.Abinova.abwn</string>
				<string>io.github.janos_szenfner.Abinova.abw</string>
			</array>
		</dict>
		<dict>
			<key>CFBundleTypeName</key>
			<string>Word Document</string>
			<key>CFBundleTypeRole</key>
			<string>Editor</string>
			<key>LSHandlerRank</key>
			<string>Alternate</string>
			<key>LSItemContentTypes</key>
			<array>
				<string>org.openxmlformats.wordprocessingml.document</string>
				<string>com.microsoft.word.doc</string>
			</array>
		</dict>
		<dict>
			<key>CFBundleTypeName</key>
			<string>OpenDocument Text</string>
			<key>CFBundleTypeRole</key>
			<string>Editor</string>
			<key>LSHandlerRank</key>
			<string>Alternate</string>
			<key>LSItemContentTypes</key>
			<array>
				<string>org.oasis-open.opendocument.text</string>
			</array>
		</dict>
		<dict>
			<key>CFBundleTypeName</key>
			<string>Rich Text Document</string>
			<key>CFBundleTypeRole</key>
			<string>Editor</string>
			<key>LSHandlerRank</key>
			<string>Alternate</string>
			<key>LSItemContentTypes</key>
			<array>
				<string>public.rtf</string>
			</array>
		</dict>
	</array>
	<key>UTExportedTypeDeclarations</key>
	<array>
		<dict>
			<key>UTTypeIdentifier</key>
			<string>io.github.janos_szenfner.Abinova.abwn</string>
			<key>UTTypeDescription</key>
			<string>Abinova Document</string>
			<key>UTTypeConformsTo</key>
			<array>
				<string>public.data</string>
			</array>
			<key>UTTypeTagSpecification</key>
			<dict>
				<key>public.filename-extension</key>
				<array>
					<string>abwn</string>
				</array>
			</dict>
		</dict>
		<dict>
			<key>UTTypeIdentifier</key>
			<string>io.github.janos_szenfner.Abinova.abw</string>
			<key>UTTypeDescription</key>
			<string>AbiWord/Abinova Document</string>
			<key>UTTypeConformsTo</key>
			<array>
				<string>public.data</string>
			</array>
			<key>UTTypeTagSpecification</key>
			<dict>
				<key>public.filename-extension</key>
				<array>
					<string>abw</string>
					<string>zabw</string>
				</array>
			</dict>
		</dict>
	</array>
</dict>
</plist>
EOF

# ---------------------------------------------------------- verify
if [ $verify -eq 1 ]; then
	echo "build-macos: verifying install names resolve in-bundle"
	find "$macos" "$fw" -type f > "$walklist"
	while IFS= read -r f; do
		is_macho "$f" || continue
		dep_list "$f" > "$depsfile"
		while IFS= read -r d; do
			case "$d" in
			@executable_path/../Frameworks/*)
				base=${d##*/}
				[ -f "$fw/$base" ] || {
					echo "  FAIL: ${f##*/} -> $d (missing)" >&2
					echo x >> "$failfile" ; } ;;
			/System/*|/usr/lib/*) : ;;
			*)
				echo "  FAIL: ${f##*/} -> $d (outside bundle)" >&2
				echo x >> "$failfile" ;;
			esac
		done < "$depsfile"
	done < "$walklist"
	if [ ! -f "$failfile" ]; then
		echo "build-macos: otool resolves only bundled+system deps — PASS"
	else
		echo "build-macos: verification FAILED (see above)" >&2
		exit 1
	fi
	if [ -s "$thinfile" ]; then
		echo "build-macos: WARNING single-arch deps in universal bundle:" >&2
		sort -u "$thinfile" | sed 's/^/  /' >&2
	fi
fi

# ------------------------------------------------------------- codesign
# arm64 Mach-O refuses to launch unsigned at all — ad-hoc sign every
# Mach-O inside-out, then the bundle.  Developer-ID + notarization is
# PACK09's layer.
if [ $sign -eq 1 ]; then
	if command -v codesign >/dev/null 2>&1; then
		find "$fw" "$macos" -type f | while read -r f; do
			is_macho "$f" && codesign -s - --force "$f" 2>/dev/null || true
		done
		codesign -s - --force "$appdir" 2>/dev/null || true
		echo "build-macos: ad-hoc codesigned (PACK09 adds Developer-ID)"
	else
		echo "build-macos: WARNING codesign not found —" >&2
		echo "  the bundle will not launch on Apple Silicon" >&2
	fi
fi

archlabel=$host_arch
[ $universal -eq 1 ] && archlabel="$host_arch+$other_arch"

cat <<EOF

Bundle ready: $appdir
  run:     open $appdir
  or:      $macos/abinova
  arch:    $archlabel
  size:    $(du -sh "$appdir" 2>/dev/null | cut -f1)
EOF
