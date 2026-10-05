#!/bin/sh
# build-windows-msys2.sh — set up dependencies and build Abinova for
# Windows inside an MSYS2 (MINGW64/UCRT64) shell, and optionally
# assemble a self-contained one-folder bundle.
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
#   tools/build-windows-msys2.sh --bundle        # or --bundle-only
#
# Usage: build-windows-msys2.sh [--prefix DIR] [--jobs N] [--skip-deps]
#                               [--bundle[=DIR]] [--bundle-only]
#                               [--stage DIR] [--dll-dirs D1[:D2...]]
#                               [--no-verify]
#
# Bundle stage (--bundle):
#   Stages 'make install' into a DESTDIR and assembles a relocatable
#   folder (default dist/abinova-<ver>-windows-<arch>):
#
#     bin/abinova.exe                entry point (console subsystem —
#     bin/abinova.exe.manifest       keeps --to=pdf working in cmd;
#                                    declares Win10 compat +
#                                    longPathAware for >MAX_PATH
#                                    document paths)
#     bin/*.dll                      every non-system DLL in the
#                                    import closure of the exe,
#                                    libabinova, the pixbuf loaders
#                                    and the enchant backends — walk
#                                    uses `objdump -p` "DLL Name"
#                                    entries to a fixpoint.  Windows
#                                    resolves DLLs from the exe's
#                                    directory first, so "beside the
#                                    exe" is the whole trick.
#     artwork/ fonts/ help/ ...      datadir flattened to the bundle
#                                    root — the PORT10 exe-path
#                                    walk-up finds artwork/ with no
#                                    env vars
#     share/glib-2.0/schemas/gschemas.compiled
#     lib/gdk-pixbuf-2.0/<ver>/loaders/*.dll + loaders.cache
#                                    (cache paths made cache-relative —
#                                    relocatable)
#     lib/enchant-2/*.dll            spellcheck backends
#     lib/gio/modules/*.dll          GIO modules incl. the libgiognutls
#                                    TLS backend (PACK06)
#     lib/gtk-4.0/<ver>/             immodules/media/printbackends
#     lib/gstreamer-1.0/*.dll        media plugins
#     certs/ca-certificates.crt      CA store for the update check
#     BUNDLE-INFO.txt                provenance + bundled-dll list
#
#   The verify pass re-walks every PE file in the bundle and fails if
#   any import name resolves outside the bundle other than the declared
#   Windows system set (kernel32/ucrtbase/api-ms-win-*/...).  GLib and
#   friends locate share/ and lib/ relative to their own DLL's prefix
#   (bin/ -> <bundle>/), so the structure mirrors an MSYS2 prefix.
#
#   GIO modules (the libgiognutls TLS backend), the CA-cert bundle,
#   GTK4 module dirs and GStreamer plugins are staged by the PACK06
#   section below; the app's runtime-module setup points GIO_EXTRA_
#   MODULES / GTK_PATH / GST_PLUGIN_SYSTEM_PATH_1_0 at them and loads
#   certs/ca-certificates.crt for the update check.  hunspell/hyph
#   dictionaries and fontconfig/fonts are PACK07's layer.  PACK09:
#   Authenticode-sign the bundle via dist/sign-windows.sh (runs when
#   ABINOVA_SIGN_SHA1 or ABINOVA_SIGN_PFX is set) and build a real
#   installer with dist/abinova-setup.nsi under makensis (auto-run
#   when makensis is on PATH).
#
#   Cross-compile note: MXE (mxe.cc) is a viable CI alternative for
#   producing abinova.exe without a Windows box
#   (x86_64-w64-mingw32.shared targets carry the whole GTK4 stack;
#   configure --host=x86_64-w64-mingw32...).  The collection logic
#   here is plain POSIX sh + objdump, which binutils builds for PE
#   targets too, so the same stage can collect an MXE-produced tree:
#   stage the install yourself (or point --stage at it) and pass the
#   sysroot bin dirs via --dll-dirs.
#
# Tested against: MSYS2 UCRT64 (x86_64), GTK 4.x.  A real Windows
# run leg is a PACK08 runner item.

set -e

prefix=""
jobs=$(nproc 2>/dev/null || echo 4)
skip_deps=0
skip_build=0
bundle=0
outdir=""
stage=""
dll_dirs=""
verify=1

while [ $# -gt 0 ]; do
	case "$1" in
	--prefix)    prefix=$2; shift 2 ;;
	--prefix=*)  prefix=${1#*=}; shift ;;
	--jobs|-j)   jobs=$2; shift 2 ;;
	--jobs=*|-j=*) jobs=${1#*=}; shift ;;
	--skip-deps) skip_deps=1; shift ;;
	--bundle)    bundle=1; shift ;;
	--bundle=*)  bundle=1; outdir=${1#*=}; shift ;;
	--bundle-only) bundle=1; skip_deps=1; skip_build=1; shift ;;
	--stage)     stage=$2; shift 2 ;;
	--stage=*)   stage=${1#*=}; shift ;;
	--dll-dirs)  dll_dirs=$2; shift 2 ;;
	--dll-dirs=*) dll_dirs=${1#*=}; shift ;;
	--no-verify) verify=0; shift ;;
	-h|--help)
		sed -n '2,73p' "$0"; exit 0 ;;
	*) echo "build-windows: unknown option $1" >&2; exit 1 ;;
	esac
done

top=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

case "${MSYSTEM:-}" in
	MINGW64|UCRT64|CLANG64|CLANGARM64|MINGW32) ;;
	*)
		if [ $bundle -eq 1 ] && [ $skip_build -eq 1 ]; then
			echo "build-windows: WARNING MSYSTEM='${MSYSTEM:-unset}'," >&2
			echo "  not an MSYS2 shell — continuing anyway because" >&2
			echo "  --bundle-only only collects files (MXE/cross" >&2
			echo "  mode); dependency install and compile are off." >&2
		else
			echo "build-windows: run this inside an MSYS2 MINGW64/UCRT64 shell" >&2
			echo "(MSYSTEM is '${MSYSTEM:-unset}', expected MINGW64 or UCRT64)" >&2
			exit 1
		fi ;;
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
		"${pkgpfx}-glib-networking" \
		"${pkgpfx}-ca-certificates" \
		"${pkgpfx}-gst-plugins-base" \
		"${pkgpfx}-gst-plugins-good" \
		"${pkgpfx}-boost" \
		"${pkgpfx}-glib2-devel" \
		perl make
fi

# -------------------------------------------------------- configure
bdir="$top/build-windows"

if [ $skip_build -eq 0 ]; then
	cfg="$top/configure"
	if [ ! -f "$cfg" ] || [ "$top/configure.ac" -nt "$cfg" ]; then
		( cd "$top" && autoreconf -f -i )
	fi

	args="--disable-maintainer-mode"
	[ -n "$prefix" ] && args="$args --prefix=$prefix"

	mkdir -p "$bdir"
	cd "$bdir"

	# shellcheck disable=SC2086
	"$cfg" $args "$@"

	# ------------------------------------------------------------ build
	make -j"$jobs"
fi

if [ $bundle -eq 0 ]; then
	cat <<EOF

Abinova built in $bdir
Run it from the build tree with:

  ABINOVA_DATADIR="$(cygpath -w "$top" 2>/dev/null || echo "$top")" $bdir/src/abinova.exe

install with:  (cd $bdir && make install)
bundle with:   $0 --bundle        # self-contained folder, see --help

EOF
	exit 0
fi

# ==================================================== bundle stage
echo "build-windows: assembling one-folder bundle"

# objdump is the only tool the walk needs; MSYS2's binutils build (and
# any binutils with pei-x86-64 in -i output, e.g. MXE's) reads PE.
command -v objdump >/dev/null 2>&1 || {
	echo "build-windows: objdump not found — install ${pkgpfx}-toolchain" >&2
	exit 1; }

vmaj=$(sed -n 's/^m4_define(\[abi_version_major\], \[\(.*\)\])/\1/p' "$top/configure.ac")
vmin=$(sed -n 's/^m4_define(\[abi_version_minor\], \[\(.*\)\])/\1/p' "$top/configure.ac")
vmic=$(sed -n 's/^m4_define(\[abi_version_micro\], \[\(.*\)\])/\1/p' "$top/configure.ac")
version=$vmaj.$vmin.$vmic
series=$vmaj.$vmin
arch=${MSYSTEM_CARCH:-$(uname -m 2>/dev/null || echo x86_64)}

# --------------------------------------------------- staged install
# $stage points at a DESTDIR-staged 'make install' tree.  With
# --bundle-only an existing stage is reused (marker file); MXE users
# pass --stage DIR pointing at their own staging.
if [ -z "$stage" ]; then
	stage="$bdir/stage"
	if [ $skip_build -eq 0 ] || [ ! -f "$stage.installed" ]; then
		rm -rf "$stage"
		mkdir -p "$bdir"
		echo "build-windows: staging 'make install' into $stage"
		make -C "$bdir" install DESTDIR="$stage" > "$bdir/install.log" 2>&1 || {
			tail -n 20 "$bdir/install.log" >&2
			echo "build-windows: make install failed (log: $bdir/install.log)" >&2
			exit 1; }
		: > "$stage.installed"
	fi
fi

exe=$(find "$stage" -type f -name abinova.exe 2>/dev/null | head -n1)
[ -n "$exe" ] || {
	echo "build-windows: no abinova.exe under stage $stage" >&2
	echo "  (run without --bundle-only first, or pass --stage DIR)" >&2
	exit 1; }
spref=$(dirname "$(dirname "$exe")")
datasrc="$spref/share/abinova-$series"
[ -d "$datasrc" ] || {
	echo "build-windows: datadir $datasrc missing from stage" >&2
	exit 1; }

[ -n "$outdir" ] || outdir="$top/dist/abinova-$version-windows-$arch"
rm -rf "$outdir"
bbin="$outdir/bin"
mkdir -p "$bbin"

cp -aL "$exe" "$bbin/abinova.exe"

# every DLL the staged install produced (libabinova, anything else we
# ship) belongs beside the exe — the Windows loader looks there first
find "$stage" -type f -name '*.dll' | while IFS= read -r d; do
	cp -aL "$d" "$bbin/${d##*/}"
done

# datadir contents -> bundle root (exe-path artwork walk-up finds it)
cp -a "$datasrc/." "$outdir/"

# --------------------------------------------------------- helpers
# Bookkeeping that must survive pipelines lives in temp files — a
# `cmd | while read` loop runs in a subshell, so variable updates
# inside it are lost.
walklist=$(mktemp "${TMPDIR:-/tmp}/abinova-walk.XXXXXX")
depsfile=$(mktemp "${TMPDIR:-/tmp}/abinova-deps.XXXXXX")
warnfile=$(mktemp "${TMPDIR:-/tmp}/abinova-warn.XXXXXX")
failfile=$(mktemp "${TMPDIR:-/tmp}/abinova-fail.XXXXXX")
copiedmark=$(mktemp "${TMPDIR:-/tmp}/abinova-copied.XXXXXX")
rm -f "$copiedmark" "$failfile"
trap 'rm -f "$walklist" "$depsfile" "$warnfile" "$failfile" "$copiedmark"' EXIT

is_pe() { # $1 = file; PE images start with 'MZ'
	case "$(od -An -tx1 -N2 "$1" 2>/dev/null | tr -d ' \n')" in
	4d5a) return 0 ;;
	esac
	return 1
}

dep_names() { # imported DLL names of PE $1, one per line
	objdump -p "$1" 2>/dev/null | \
		sed -n 's/^[ 	]*DLL Name: \(.*\)/\1/p' | tr -d '\r'
}

is_system_dll() { # $1 = dll basename; system set ships with Windows
	case "$(echo "$1" | tr 'A-Z' 'a-z')" in
	api-ms-win-*|ext-ms-win-*|advapi32.dll|bcrypt.dll|\
	bcryptprimitives.dll|cabinet.dll|cfgmgr32.dll|combase.dll|\
	comctl32.dll|comdlg32.dll|crypt32.dll|cryptbase.dll|cryptsp.dll|\
	d2d1.dll|d3d*.dll|dbghelp.dll|dhcpcsvc*.dll|dnsapi.dll|\
	dwmapi.dll|dwrite.dll|dxgi.dll|dxva2.dll|gdi32.dll|\
	gdi32full.dll|glu32.dll|hid.dll|iertutil.dll|imm32.dll|\
	iphlpapi.dll|kernel32.dll|kernel.appcore.dll|kernelbase.dll|\
	mf*.dll|msasn1.dll|msctf.dll|msimg32.dll|msvcp_win.dll|\
	msvcrt.dll|ncrypt.dll|netapi32.dll|netutils.dll|normaliz.dll|\
	nsi.dll|ntdll.dll|ole32.dll|oleacc.dll|oleaut32.dll|\
	opengl32.dll|powrprof.dll|profapi.dll|psapi.dll|ras*.dll|\
	rpcrt4.dll|rsaenh.dll|sechost.dll|secur32.dll|setupapi.dll|\
	shcore.dll|shell32.dll|shlwapi.dll|sspicli.dll|ucrtbase.dll|\
	umpdc.dll|urlmon.dll|user32.dll|userenv.dll|usp10.dll|\
	uxtheme.dll|version.dll|winhttp.dll|wininet.dll|winmm.dll|\
	winscard.dll|winspool.drv|wintrust.dll|wldap32.dll|\
	ws2_32.dll|wsock32.dll|wtsapi32.dll)
		return 0 ;;
	esac
	return 1
}

in_bundle() { # $1 = dll basename: already inside $outdir anywhere?
	find "$outdir" -type f -iname "$1" -print -quit 2>/dev/null | \
		grep -q .
}

find_dll() { # $1 = dll basename -> first match in the search dirs
	base=$1
	oldifs=$IFS; IFS=:
	for d in $bbin $dll_dirs \
		"${MINGW_PREFIX:+$MINGW_PREFIX/bin}" \
		"${MINGW_PREFIX:+$MINGW_PREFIX/lib}"; do
		[ -n "$d" ] || continue
		if [ -f "$d/$base" ]; then
			echo "$d/$base"; IFS=$oldifs; return 0
		fi
		f=$(find "$d" -maxdepth 1 -type f -iname "$base" 2>/dev/null \
			| head -n1)
		if [ -n "$f" ]; then
			echo "$f"; IFS=$oldifs; return 0
		fi
	done
	IFS=$oldifs
	return 1
}

# --------------------------------------------- gdk-pixbuf loaders
pixdir=$(find "${MINGW_PREFIX:-/nonexistent}/lib" -type d \
	-path '*gdk-pixbuf-2.0/*/loaders' 2>/dev/null | sort | tail -n1)
if [ -n "$pixdir" ]; then
	# keep the gdk-pixbuf-2.0/<ver>/loaders tail under the bundle's lib/
	relload=$(echo "$pixdir" | sed 's|.*/\(gdk-pixbuf-2.0/.*\)|\1|')
	bload="$outdir/lib/$relload"
	mkdir -p "$bload"
	for dll in "$pixdir"/*.dll; do
		[ -f "$dll" ] || continue
		cp -aL "$dll" "$bload/${dll##*/}"
	done
	query=$(command -v gdk-pixbuf-query-loaders 2>/dev/null || true)
	[ -n "$query" ] || query=$(find "${MINGW_PREFIX:-/nonexistent}/bin" \
		-type f -name 'gdk-pixbuf-query-loaders*' 2>/dev/null | head -n1)
	if [ -n "$query" ]; then
		# module paths relative to the cache's own dir — relocatable
		"$query" "$bload"/*.dll 2>/dev/null | \
			sed "s|\"$bload/|\"loaders/|g" \
			> "$bload/../loaders.cache" || true
		if [ ! -s "$bload/../loaders.cache" ]; then
			rm -f "$bload/../loaders.cache"
			echo "build-windows: WARNING gdk-pixbuf-query-loaders" >&2
			echo "  produced no cache — loaders copied but image" >&2
			echo "  formats may not resolve in the bundle" >&2
		fi
	else
		echo "build-windows: WARNING no gdk-pixbuf-query-loaders —" >&2
		echo "  loaders copied but no loaders.cache generated" >&2
	fi
else
	echo "build-windows: WARNING no gdk-pixbuf loaders dir found" >&2
fi

# ------------------------------------------------ gschemas.compiled
bschema="$outdir/share/glib-2.0/schemas"
gsrc="${MINGW_PREFIX:-/nonexistent}/share/glib-2.0/schemas"
if [ -d "$gsrc" ]; then
	mkdir -p "$bschema"
	# all *.xml — *.enums.xml ship the enum defs the gschemas need;
	# *.gschema.override files stay out (they carry local tweaks only)
	cp -a "$gsrc"/*.xml "$bschema/" 2>/dev/null || true
	if command -v glib-compile-schemas >/dev/null 2>&1; then
		glib-compile-schemas --strict --targetdir="$bschema" \
			"$bschema" 2>/dev/null || \
			glib-compile-schemas --targetdir="$bschema" "$bschema"
		rm -f "$bschema"/*.xml
	else
		echo "build-windows: WARNING glib-compile-schemas missing" >&2
	fi
else
	echo "build-windows: WARNING no glib schemas dir found" >&2
fi

# ------------------------------------------------- enchant backends
# spellcheck providers live in lib/enchant-2 beside the prefix — the
# layout survives because enchant resolves its provider dir relative
# to its own DLL (prefix/bin -> prefix/lib/enchant-2).
endir="${MINGW_PREFIX:-/nonexistent}/lib/enchant-2"
if [ -d "$endir" ]; then
	mkdir -p "$outdir/lib/enchant-2"
	for dll in "$endir"/*.dll; do
		[ -f "$dll" ] || continue
		cp -aL "$dll" "$outdir/lib/enchant-2/${dll##*/}"
	done
else
	echo "build-windows: WARNING no enchant backends dir found" >&2
fi

# ----------------------------------------------------- GIO modules
# libgiognutls is the TLS backend the update check needs.  The app's
# runtime-module setup exports GIO_EXTRA_MODULES=<root>/lib/gio/modules;
# on Windows glib would also auto-probe <prefix>/lib/gio/modules via
# its own DLL location, so the layout matters either way.
giodir=$(find "${MINGW_PREFIX:-/nonexistent}/lib" -type d \
	-path '*/gio/modules' 2>/dev/null | sort | tail -n1)
if [ -n "$giodir" ]; then
	bgio="$outdir/lib/gio/modules"
	mkdir -p "$bgio"
	for dll in "$giodir"/*.dll; do
		[ -f "$dll" ] || continue
		cp -aL "$dll" "$bgio/${dll##*/}"
	done
	query=$(command -v gio-querymodules 2>/dev/null || true)
	[ -n "$query" ] && "$query" "$bgio" >/dev/null 2>&1 || true
else
	echo "build-windows: WARNING no gio modules dir found" >&2
	echo "  (pacman ${pkgpfx}-glib-networking provides the TLS backend)" >&2
fi

# -------------------------------------------------------- CA bundle
# xap_UpdateCheck loads certs/ca-certificates.crt relative to the
# datadir root into a GTlsFileDatabase per connection.
for c in "${MINGW_PREFIX:-/nonexistent}/etc/ssl/certs/ca-bundle.crt" \
	"${MINGW_PREFIX:-/nonexistent}/etc/ssl/cert.pem"; do
	if [ -f "$c" ]; then
		mkdir -p "$outdir/certs"
		cp -aL "$c" "$outdir/certs/ca-certificates.crt"
		break
	fi
done
[ -f "$outdir/certs/ca-certificates.crt" ] || \
	echo "build-windows: WARNING no CA bundle found" >&2

# --------------------------------------------------- GTK4 modules
# immodules/media/printbackends subdirs of lib/gtk-4.0/<ver>; the app
# exports GTK_PATH=<root>/lib/gtk-4.0.
gtkdir=$(find "${MINGW_PREFIX:-/nonexistent}/lib" -type d \
	-name printbackends -path '*gtk-4.0*' 2>/dev/null | sort | tail -n1)
if [ -n "$gtkdir" ]; then
	gtkdir=$(dirname "$gtkdir")
	relgtk=$(echo "$gtkdir" | sed 's|.*/\(gtk-4.0/.*\)|\1|')
	for sub in "$gtkdir"/*/; do
		[ -d "$sub" ] || continue
		mkdir -p "$outdir/lib/$relgtk"
		cp -aL "${sub%/}" "$outdir/lib/$relgtk/"
	done
else
	echo "build-windows: WARNING no gtk-4.0 module dir found" >&2
fi

# ---------------------------------------------------- GStreamer
# plugins for GTK4's libmedia-gstreamer.dll; the app exports
# GST_PLUGIN_SYSTEM_PATH_1_0=<root>/lib/gstreamer-1.0.
gstdir=$(find "${MINGW_PREFIX:-/nonexistent}/lib" -type d \
	-name 'gstreamer-1.0' 2>/dev/null | sort | tail -n1)
if [ -n "$gstdir" ]; then
	mkdir -p "$outdir/lib/gstreamer-1.0"
	for dll in "$gstdir"/*.dll; do
		[ -f "$dll" ] || continue
		cp -aL "$dll" "$outdir/lib/gstreamer-1.0/${dll##*/}"
	done
else
	echo "build-windows: WARNING no gstreamer-1.0 dir found" >&2
fi

# ------------------------------------------------- PACK07 data files
# hunspell dictionaries + hyphenation patterns -> <root>/hunspell and
# <root>/hyphen.  The app appends the bundle root to XDG_DATA_DIRS,
# PORT04's dictionaryDirs() and ut_hyphen's s_candidateDirs() probe
# <install-dir>/{hunspell,hyphen,share/*} explicitly, and the enchant
# provider sees them via its own XDG/prefix-dir walk.
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
hdir="${MINGW_PREFIX:-/nonexistent}/share/hunspell"
yd="${MINGW_PREFIX:-/nonexistent}/share/hyphen"
copy_dicts "$outdir/hunspell" spell "$hdir"
copy_dicts "$outdir/hyphen" hyph "$yd" "$hdir"
[ -f "$outdir/hunspell/en_US.dic" ] || \
	echo "build-windows: WARNING no English hunspell dictionary" >&2
	echo "  staged (pacman ${pkgpfx:-<prefix>}-hunspell-en provides it)" >&2

# fontconfig — the DLL finds <root>/etc/fonts relative to its own
# module path on Windows, and the app exports FONTCONFIG_FILE when a
# bundled fonts.conf exists; MSYS2's stock config uses the
# WINDOWSFONTDIR/USER/CACHEDIR tokens so it relocates cleanly
if [ -d "${MINGW_PREFIX:-/nonexistent}/etc/fonts" ]; then
	mkdir -p "$outdir/etc/fonts"
	# cp -aL resolves conf.d's symlinks into real files
	cp -aL "${MINGW_PREFIX}/etc/fonts/." "$outdir/etc/fonts/"
else
	echo "build-windows: WARNING no ${MINGW_PREFIX:-<prefix>}/etc/fonts" >&2
	echo "  — bundled fontconfig falls back to its built-in defaults" >&2
fi

# --------------------------------------------- DLL closure walk
# Iterate to a fixpoint: every PE already in the bundle feeds its
# import list; each copied DLL may pull more deps.  All resolved deps
# land in bin/ — Windows searches the exe's directory (and only the
# system allowlist, never the bundle lib subdirs) for a DLL's own
# imports.
while :; do
	find "$outdir" -type f \( -name '*.exe' -o -name '*.dll' \) \
		> "$walklist"
	while IFS= read -r f; do
		is_pe "$f" || continue
		dep_names "$f" > "$depsfile"
		while IFS= read -r d; do
			[ -n "$d" ] || continue
			is_system_dll "$d" && continue
			in_bundle "$d" && continue
			src=$(find_dll "$d") || {
				grep -qixF "$d" "$warnfile" 2>/dev/null || {
					echo "build-windows: WARNING unresolved dep $d" >&2
					echo "$d" >> "$warnfile"; }
				continue; }
			cp -aL "$src" "$bbin/${src##*/}"
			: >> "$copiedmark"
		done < "$depsfile"
	done < "$walklist"
	[ -f "$copiedmark" ] || break
	rm -f "$copiedmark"
done

echo "build-windows: $(find "$bbin" -maxdepth 1 -name '*.dll' | wc -l | tr -d ' ') dlls beside bin/abinova.exe"

# ---------------------------------------------------- exe manifest
# Side-by-side manifest: the MSYS2 toolchain does not embed a manifest
# into abinova.exe, so Windows honours this <exe>.manifest file.
# supportedOS = Windows 10/11 compat GUID; longPathAware lifts the
# 260-char path cap for document locations (matches the Win10 floor
# _WIN32_WINNT=0x0A00 that PORT09 pinned).
cat > "$bbin/abinova.exe.manifest" <<EOF
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
	<assemblyIdentity type="win32"
		name="io.github.janos_szenfner.Abinova"
		version="$version.0" processorArchitecture="*"/>
	<compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1">
		<application>
			<!-- Windows 10 / Windows 11 -->
			<supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"/>
		</application>
	</compatibility>
	<application xmlns="urn:schemas-microsoft-com:asm.v3">
		<windowsSettings>
			<longPathAware xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">true</longPathAware>
		</windowsSettings>
	</application>
</assembly>
EOF

# bundle icon for a future installer / shortcut (PACK09)
[ -f "$top/icons/abinova.ico" ] && cp "$top/icons/abinova.ico" "$outdir/abinova.ico"

# ------------------------------------------------------- BUNDLE-INFO
{
	echo "Abinova $version Windows bundle"
	echo "built:    $(date -u +%Y-%m-%dT%H:%M:%SZ)"
	echo "host:     $(uname -a 2>/dev/null)"
	echo "msys:     ${MSYSTEM:-none} (arch $arch)"
	echo "stage:    $stage"
	echo
	echo "bin/      abinova.exe + abinova.exe.manifest + DLL closure"
	echo "share/    glib-2.0/schemas/gschemas.compiled"
	echo "lib/      gdk-pixbuf-2.0 loaders+cache, enchant-2 backends,"
	echo "          gio/modules (TLS), gtk-4.0 modules, gstreamer-1.0"
	echo "certs/    ca-certificates.crt (update-check CA store)"
	echo "hunspell/ spelling dictionaries; hyphen/ hyph_*.dic patterns"
	echo "etc/fonts/ private fontconfig (FONTCONFIG_FILE at startup)"
	echo "root      datadir contents (artwork/, fonts/, ...)"
	echo
	echo "Bundled DLLs:"
	find "$bbin" -maxdepth 1 -name '*.dll' -printf '  %f\n' | sort
} > "$outdir/BUNDLE-INFO.txt"

# ---------------------------------------------------------- verify
if [ $verify -eq 1 ]; then
	echo "build-windows: verifying dll imports resolve in-bundle"
	find "$outdir" -type f \( -name '*.exe' -o -name '*.dll' \) \
		> "$walklist"
	while IFS= read -r f; do
		is_pe "$f" || continue
		dep_names "$f" > "$depsfile"
		while IFS= read -r d; do
			[ -n "$d" ] || continue
			is_system_dll "$d" && continue
			in_bundle "$d" || {
				echo "  FAIL: ${f##*/} -> $d (unresolved)" >&2
				echo x >> "$failfile"; }
		done < "$depsfile"
	done < "$walklist"
	if [ ! -f "$failfile" ]; then
		echo "build-windows: every import resolves inside the bundle" >&2
		echo "  or the Windows system set — PASS" >&2
	else
		echo "build-windows: verification FAILED (see above)" >&2
		exit 1
	fi
fi

# --------------------------------------------- signing + installer
# PACK09 layer.  Authenticode signing is opt-in (real certs needed):
# set ABINOVA_SIGN_SHA1=<thumbprint> or ABINOVA_SIGN_PFX=<file>
# (+ABINOVA_SIGN_PFX_PASSWORD) and dist/sign-windows.sh signs every PE
# in the bundle via signtool.  dist/abinova-setup.nsi turns the folder
# into a proper installer (file associations, shortcuts, Add/Remove
# Programs, uninstaller); it runs automatically when makensis is on
# PATH, or manually:
#   makensis -DVERSION=$version -DARCH=$arch -DSRCDIR=<bundle> \
#     dist/abinova-setup.nsi
# See dist/SIGNING.md for the full signing recipe.
winout=$(cygpath -w "$outdir" 2>/dev/null || echo "$outdir")
if [ -n "${ABINOVA_SIGN_SHA1:-}${ABINOVA_SIGN_PFX:-}" ]; then
	sh "$top/dist/sign-windows.sh" "$outdir"
else
	echo "build-windows: bundle left unsigned — set ABINOVA_SIGN_SHA1" >&2
	echo "  or ABINOVA_SIGN_PFX(+_PASSWORD) for Authenticode signing" >&2
	echo "  (dist/sign-windows.sh; see dist/SIGNING.md)" >&2
fi
if command -v makensis >/dev/null 2>&1; then
	signcmd=""
	if [ -n "${ABINOVA_SIGN_SHA1:-}${ABINOVA_SIGN_PFX:-}" ]; then
		# let NSIS !finalize/!uninstfinalize sign the installer and
		# the embedded uninstaller with the same certificate
		_signtool=${ABINOVA_SIGNTOOL:-signtool.exe}
		signcmd="$_signtool sign /fd sha256 /td sha256 /tr \
${ABINOVA_SIGN_TSA:-http://timestamp.digicert.com}"
		if [ -n "${ABINOVA_SIGN_SHA1:-}" ]; then
			signcmd="$signcmd /sha1 $ABINOVA_SIGN_SHA1"
		else
			signcmd="$signcmd /f $ABINOVA_SIGN_PFX"
			[ -n "${ABINOVA_SIGN_PFX_PASSWORD:-}" ] && \
				signcmd="$signcmd /p $ABINOVA_SIGN_PFX_PASSWORD"
		fi
		signcmd="$signcmd /d Abinova"
	fi
	makensis -DVERSION="$version" -DARCH="$arch" \
		-DSRCDIR="$winout" ${signcmd:+-DSIGNCMD="$signcmd"} \
		"$top/dist/abinova-setup.nsi"
else
	echo "build-windows: no makensis — to build the installer run:" >&2
	echo "  makensis -DVERSION=$version -DARCH=$arch \\" >&2
	echo "    -DSRCDIR=\"$winout\" dist/abinova-setup.nsi" >&2
fi

cat <<EOF

Bundle ready: $outdir
  run:     ${outdir##*/}/bin/abinova.exe
  arch:    $arch
  size:    $(du -sh "$outdir" 2>/dev/null | cut -f1)

Windows run-leg is unverified here — smoke it on a Windows box or
runner (PACK08): bin/abinova.exe, open a .docx, --to=pdf.
EOF
