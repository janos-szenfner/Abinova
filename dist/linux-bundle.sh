#!/bin/sh
# linux-bundle.sh — assemble a self-contained Linux bundle of Abinova.
#
# Produces a relocatable directory (default
# dist/abinova-<ver>-linux-<arch>):
#
#   bin/abinova                            ELF, RUNPATH=$ORIGIN/../lib
#   lib/libabinova-<series>.so             RUNPATH=$ORIGIN/../lib
#   lib/<soname>                           every non-system entry of the
#                                          PACK01 closure (dist/BUNDLED-DEPS.md)
#                                          plus extra deps of the pixbuf
#                                          loaders, all RUNPATH=$ORIGIN/../lib
#   lib/gdk-pixbuf-2.0/<ver>/loaders/*.so  pixbuf modules, RUNPATH back to lib/
#   lib/gdk-pixbuf-2.0/<ver>/loaders.cache (cache-relative module paths —
#                                          the app rewrites them absolute in
#                                          $XDG_CACHE_HOME at startup)
#   lib/gio/modules/*.so + giomodule.cache PACK06: GIO modules incl. the
#                                          libgiognutls TLS backend; the app
#                                          exports GIO_EXTRA_MODULES at startup
#   lib/gtk-4.0/<ver>/{immodules,media,printbackends}/
#                                          GTK4 runtime modules; located via
#                                          the app's GTK_PATH export
#   lib/gstreamer-1.0/*.so                 plugins for GTK4's media module;
#                                          located via GST_PLUGIN_SYSTEM_PATH_1_0
#   lib/enchant-2/*.so                     spellcheck backends; the app
#                                          self-loads them when the enchant
#                                          broker finds none (its compiled-in
#                                          provider dir is not relocatable on
#                                          this distro). A copy also lands in
#                                          lib/<multiarch>/enchant-2 for
#                                          relocatable enchant builds
#   share/enchant-2/enchant.ordering       provider priorities, where present
#   share/glib-2.0/schemas/gschemas.compiled
#   certs/ca-certificates.crt              CA bundle for the update check —
#                                          xap_UpdateCheck loads it via a
#                                          GTlsFileDatabase on each connection
#   hunspell/*.aff,*.dic                   spelling dictionaries copied from the
#                                          host's hunspell/myspell dirs; the app
#                                          appends the bundle root to
#                                          XDG_DATA_DIRS and probes this dir in
#                                          PORT04's dictionaryDirs()
#   hyphen/hyph_*.dic                      hyphenation patterns for RBN06's
#                                          auto-hyphenation (ut_hyphen.cpp)
#   share/applications + share/icons       desktop files, kept for PACK09
#   artwork/ fonts/ help/ mime-info/ omml_xslt/ system.profile templates/ xsltml/
#                                          the datadir is flattened to the
#                                          bundle root so the app's existing
#                                          exe-path walk-up finds artwork/ with
#                                          no wrapper script or env vars
#   BUNDLE-INFO.txt                        provenance + bundled-lib manifest
#
# $ORIGIN/../lib is used on every ELF: for bin/abinova it reaches lib/,
# and for a lib inside lib/ it resolves to lib/ itself — one string
# covers both cases (nested modules get the equivalent relative path).
#
# Only libc/libm/ld.so/vdso (+ glibc-family stub sonames) are treated as
# system; everything else is copied per the PACK01 manifest.
#
# Usage:
#   dist/linux-bundle.sh [--output DIR] [--patchelf PATH] [--stage DIR]
#                        [--skip-install] [--no-strip] [--no-verify]
#                        [--print-libs]
#
# patchelf is required (for RUNPATH edits) but is a build-time-only
# tool — nothing it produces needs it at runtime.  If it is missing:
#   Debian/Ubuntu: apt install patchelf
#   from source:   g++ -O2 -o patchelf src/patchelf.cc   (single file,
#                  https://github.com/NixOS/patchelf/releases)
#
# Tested on: Debian/Ubuntu-family x86-64.  Other arches follow the same
# rules (untested).

set -e

outdir=""
patchelf=""
stage=""
skip_install=0
strip=1
verify=1
print_libs=0
selftest_docker=auto

while [ $# -gt 0 ]; do
	case "$1" in
	--output)       outdir=$2; shift 2 ;;
	--output=*)     outdir=${1#*=}; shift ;;
	--patchelf)     patchelf=$2; shift 2 ;;
	--patchelf=*)   patchelf=${1#*=}; shift ;;
	--stage)        stage=$2; shift 2 ;;
	--stage=*)      stage=${1#*=}; shift ;;
	--skip-install) skip_install=1; shift ;;
	--no-strip)     strip=0; shift ;;
	--no-verify)    verify=0; shift ;;
	--print-libs)   print_libs=1; shift ;;
	--self-test)    selftest_docker=yes; shift ;;
	--no-self-test) selftest_docker=no; shift ;;
	-h|--help)      sed -n '2,52p' "$0"; exit 0 ;;
	*) echo "linux-bundle: unknown option $1" >&2; exit 1 ;;
	esac
done

top=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

if [ "$(uname -s)" != "Linux" ]; then
	echo "linux-bundle: this script targets Linux (uname: $(uname -s))" >&2
	exit 1
fi

vmaj=$(sed -n 's/^m4_define(\[abi_version_major\], \[\(.*\)\])/\1/p' "$top/configure.ac")
vmin=$(sed -n 's/^m4_define(\[abi_version_minor\], \[\(.*\)\])/\1/p' "$top/configure.ac")
vmic=$(sed -n 's/^m4_define(\[abi_version_micro\], \[\(.*\)\])/\1/p' "$top/configure.ac")
version=$vmaj.$vmin.$vmic
series=$vmaj.$vmin
arch=$(uname -m)

[ -n "$outdir" ] || outdir="$top/dist/abinova-$version-linux-$arch"
[ -n "$stage" ]  || stage="$outdir.stage"

# ------------------------------------------------------------ patchelf
if [ -z "$patchelf" ]; then
	patchelf=${PATCHELF:-}
fi
if [ -z "$patchelf" ]; then
	patchelf=$(command -v patchelf 2>/dev/null || true)
fi
if [ -z "$patchelf" ]; then
	echo "linux-bundle: patchelf is required to set RUNPATH." >&2
	echo "  install it (apt install patchelf) or build the single-file" >&2
	echo "  source: g++ -O2 -o patchelf patchelf-*/src/patchelf.cc" >&2
	echo "  then re-run with --patchelf /path/to/patchelf" >&2
	exit 1
fi
"$patchelf" --version >/dev/null 2>&1 || {
	echo "linux-bundle: '$patchelf' does not run" >&2; exit 1; }

# ----------------------------------------------------- staged install
if [ $skip_install -eq 0 ]; then
	rm -rf "$stage"
	echo "linux-bundle: staging 'make install' into $stage"
	if ! make -C "$top" -j"$(nproc 2>/dev/null || echo 2)" install \
		DESTDIR="$stage" > "$stage.install.log" 2>&1; then
		tail -n 20 "$stage.install.log" >&2
		echo "linux-bundle: make install failed (log: $stage.install.log)" >&2
		exit 1
	fi
	rm -f "$stage.install.log"
	mkdir -p "$stage"	# in case install created nothing at all
fi

prefix_dir=$(dirname "$(dirname "$(find "$stage" -type f -name abinova \
	-path '*/bin/*' | head -n1)")")
if [ ! -f "$prefix_dir/bin/abinova" ]; then
	echo "linux-bundle: no abinova binary under $stage" >&2
	exit 1
fi
libsrc="$prefix_dir/lib"
datasrc="$prefix_dir/share/abinova-$series"
[ -d "$datasrc" ] || {
	echo "linux-bundle: datadir $datasrc missing from stage" >&2; exit 1; }

# -------------------------------------------------------- lib closure
# Parse ldd output: "  soname => /abs/path (0x...)" -> "soname path".
# Interpreter ("  /lib64/ld-linux...") and vdso lines carry no '=>'.
ldd_libs() {
	LC_ALL=C ldd "$@" 2>/dev/null | awk '
		/=> *not found/ { print "NOTFOUND " $1; next }
		/=>/ { print $1, $3 }
	' | sort -u
}

# glibc-package sonames that must stay system (they belong to the
# kernel/glibc contract — bundling a mismatched stub is actively bad).
is_system_lib() {
	case "$1" in
	libc.so*|libm.so*|libdl.so*|libpthread.so*|librt.so*|libutil.so*| \
	libresolv.so*|libnsl.so*|libanl.so*|ld-linux-*.so*|ld64.so*| \
	ld-musl-*.so*|linux-vdso*|linux-gate*)
		return 0 ;;
	esac
	return 1
}

pixdir=$(find /usr/lib /usr/lib64 -type d -path '*gdk-pixbuf-2.0/*/loaders' \
	2>/dev/null | sort | tail -n1)

# PACK06 runtime modules — loaded via dlopen/g_io_modules_* at runtime,
# invisible to ldd.  Their own NEEDED deps are folded into the closure
# below so the bundle keeps its "system = libc family only" property.
encdir=$(find /usr/lib /usr/lib64 -type d -name 'enchant-2' \
	2>/dev/null | sort | tail -n1)
giodir=$(find /usr/lib /usr/lib64 -type d -path '*/gio/modules' \
	2>/dev/null | sort | tail -n1)
# the gtk-4.0/<binary-version> dir carries immodules/media/printbackends
gtkdir=$(find /usr/lib /usr/lib64 -type d -name printbackends \
	-path '*gtk-4.0*' 2>/dev/null | sort | tail -n1)
[ -n "$gtkdir" ] && gtkdir=$(dirname "$gtkdir")
gstdir=""
for d in $(find /usr/lib /usr/lib64 -type d -name 'gstreamer-1.0' \
	2>/dev/null | sort); do
	# the plugin dir holds *.so; a gstreamer1.0/gstreamer-1.0 dir seen
	# on some distros carries only helper files — skip those
	if ls "$d"/*.so >/dev/null 2>&1; then gstdir=$d; break; fi
done

# gstreamer plugins to bundle — sinks/test plugins irrelevant to
# embedded-document playback are skipped so they can't drag whole
# toolkits (libgtk-3) or niche hw/audio libs into the dep closure
gst_plugins() {
	[ -n "$gstdir" ] || return 0
	for gst in "$gstdir"/*.so; do
		[ -f "$gst" ] || continue
		case ${gst##*/} in
		libgst1394.so|libgstaasink.so|libgstcacasink.so|libgstgtk.so|\
		libgstnavigationtest.so|libgstximagesink.so|libgstxvimagesink.so)
			continue ;;
		esac
		echo "$gst"
	done
}

# libcrypto is dlopen()'d by the .abwn crypto code (ut_abwncrypt.cpp);
# it never shows up in NEEDED, so seed it into the closure explicitly
# and copy it by hand below (ldd only lists a file's deps, not itself).
cryptosrc=""
for c in libcrypto.so.3 libcrypto.so.1.1 libcrypto.so.1.0.2 libcrypto.so; do
	cryptosrc=$(find /usr/lib /usr/lib64 -name "$c" \
		2>/dev/null | head -n1)
	[ -n "$cryptosrc" ] && break
done

all_libs=$(ldd_libs "$libsrc"/libabinova-*.so \
	${pixdir:+"$pixdir"/*.so} \
	${encdir:+"$encdir"/*.so} \
	${giodir:+"$giodir"/*.so} \
	${gtkdir:+$(find "$gtkdir" -type f -name '*.so' 2>/dev/null)} \
	$(gst_plugins) \
	${cryptosrc:+"$cryptosrc"} 2>/dev/null)
notfound=$(echo "$all_libs" | awk '/^NOTFOUND/ {print $2}')
if [ -n "$notfound" ]; then
	echo "linux-bundle: unresolved deps before bundling:" >&2
	echo "$notfound" >&2
	exit 1
fi

if [ $print_libs -eq 1 ]; then
	echo "linux-bundle: closure for libabinova-$series.so + runtime" \
		"modules (pixbuf loaders, enchant backends, gio/gtk4/gstreamer" \
		"modules, dlopen'd libcrypto)"
	echo "$all_libs" | while read -r name path; do
		if is_system_lib "$name"; then
			echo "  system  $name -> $path"
		else
			echo "  bundle  $name -> $path"
		fi
	done | sort -k2
	exit 0
fi

# ------------------------------------------------------- assemble dir
rm -rf "$outdir"
mkdir -p "$outdir/bin" "$outdir/lib"

cp -a "$prefix_dir/bin/abinova" "$outdir/bin/abinova"
cp -aL "$libsrc"/libabinova-$series.so "$outdir/lib/"

count=0
echo "$all_libs" | while read -r name path; do
	if is_system_lib "$name" || [ -f "$outdir/lib/$name" ]; then
		continue
	fi
	cp -aL "$path" "$outdir/lib/$name" || exit 1
done
count=$(find "$outdir/lib" -maxdepth 1 -type f -name '*.so*' | wc -l)
echo "linux-bundle: $count shared libs collected into lib/"

# dlopen()'d, never a NEEDED entry — copy by hand under its soname
if [ -n "$cryptosrc" ] && [ ! -f "$outdir/lib/${cryptosrc##*/}" ]; then
	cp -aL "$cryptosrc" "$outdir/lib/${cryptosrc##*/}"
fi

# datadir contents at bundle root — the app's exe-path walk-up
# (xap_UnixApp::_setAbiSuiteLibDir) discovers <root>/artwork itself
cp -a "$datasrc/." "$outdir/"

# desktop integration files ride along for the installer layer (PACK09)
mkdir -p "$outdir/share"
for d in applications icons; do
	if [ -d "$prefix_dir/share/$d" ]; then
		cp -a "$prefix_dir/share/$d" "$outdir/share/"
	fi
done

# ------------------------------------------- gdk-pixbuf loaders+cache
if [ -n "$pixdir" ]; then
	# keep the gdk-pixbuf-2.0/<ver>/loaders tail regardless of the
	# host libdir prefix (/usr/lib/<triplet>, /usr/lib64, ...)
	relload=$(echo "$pixdir" | sed 's|.*/\(gdk-pixbuf-2.0/.*\)|\1|')
	bload="$outdir/lib/$relload"
	mkdir -p "$bload"
	cp -aL "$pixdir"/*.so "$bload/"
	query=$(command -v gdk-pixbuf-query-loaders 2>/dev/null || true)
	[ -n "$query" ] || query=$(find /usr/lib -type f \
		-name 'gdk-pixbuf-query-loaders*' 2>/dev/null | head -n1)
	if [ -n "$query" ]; then
		# emit module paths relative to the cache file's own dir
		# so PACK06 can point GDK_PIXBUF_MODULE_FILE at it without
		# knowing the install prefix
		"$query" "$bload"/*.so 2>/dev/null | \
			sed "s|\"$bload/|\"loaders/|g" \
			> "$bload/../loaders.cache"
	else
		echo "linux-bundle: WARNING no gdk-pixbuf-query-loaders —" >&2
		echo "  loaders copied but no loaders.cache generated" >&2
	fi
else
	echo "linux-bundle: WARNING no gdk-pixbuf loaders dir found" >&2
fi

# -------------------------------------------------- enchant backends
# Enchant dlopen()s providers from a compiled-in dir that is NOT
# relocatable on most distro builds; the app self-loads whatever sits
# in <root>/lib/enchant-2 when the broker finds nothing
# (enchant_checker.cpp).  A second copy under the host's multiarch
# libdir tail (lib/<triplet>/enchant-2) covers gnulib-style relocatable
# enchant builds, which resolve <prefix>/<libdir-tail>/enchant-2.
if [ -n "$encdir" ]; then
	mkdir -p "$outdir/lib/enchant-2"
	cp -aL "$encdir"/*.so "$outdir/lib/enchant-2/"
	mlib=$(dirname "$encdir")
	case "$mlib" in
	/usr/local/*) mlib=${mlib#/usr/local/} ;;	# /usr/local/lib -> lib
	/usr/*)       mlib=${mlib#/usr/} ;;		# /usr/lib/<t> -> lib/<t>
	*)            mlib="" ;;				# non-/usr prefix: skip mirror
	esac
	if [ -n "$mlib" ] && [ "$mlib" != "lib" ]; then
		mkdir -p "$outdir/$mlib/enchant-2"
		cp -aL "$encdir"/*.so "$outdir/$mlib/enchant-2/"
	fi
	if [ -f /usr/share/enchant-2/enchant.ordering ]; then
		mkdir -p "$outdir/share/enchant-2"
		cp -a /usr/share/enchant-2/enchant.ordering \
			"$outdir/share/enchant-2/"
	fi
else
	echo "linux-bundle: WARNING no enchant-2 backends dir found" >&2
fi

# ------------------------------------------------------ GIO modules
# libgiognutls.so is the TLS backend the update check needs; the app
# exports GIO_EXTRA_MODULES=<root>/lib/gio/modules at startup.
if [ -n "$giodir" ]; then
	bgio="$outdir/lib/gio/modules"
	mkdir -p "$bgio"
	cp -aL "$giodir"/*.so "$bgio/"
	if command -v gio-querymodules >/dev/null 2>&1; then
		gio-querymodules "$bgio" 2>/dev/null || \
			echo "linux-bundle: WARNING gio-querymodules failed" >&2
	else
		echo "linux-bundle: WARNING no gio-querymodules —" >&2
		echo "  gio modules copied but no giomodule.cache generated" >&2
	fi
else
	echo "linux-bundle: WARNING no gio modules dir found" >&2
fi

# ------------------------------------------------------- CA bundle
# xap_UpdateCheck loads certs/ca-certificates.crt into a
# GTlsFileDatabase per connection — the pack must not depend on the
# host's trust store.
cabundle=""
for c in /etc/ssl/certs/ca-certificates.crt \
	/etc/pki/tls/certs/ca-bundle.crt \
	/etc/ssl/ca-bundle.pem /etc/ssl/cert.pem; do
	if [ -f "$c" ]; then cabundle=$c; break; fi
done
if [ -n "$cabundle" ]; then
	mkdir -p "$outdir/certs"
	cp -aL "$cabundle" "$outdir/certs/ca-certificates.crt"
else
	echo "linux-bundle: WARNING no CA certificate bundle found" >&2
fi

# ------------------------------------------------- PACK07 dictionaries
# Spellcheck needs real dictionaries, not just the enchant backends
# staged above.  Whatever hunspell dictionaries the host has land at
# <root>/hunspell — found via the app's XDG_DATA_DIRS append and the
# PORT04 search-path list — and hyphenation patterns land at
# <root>/hyphen for ut_hyphen.  First occurrence wins on name
# collisions (earlier dirs shadow later ones).
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

# hyph_*.dic kept OUT of the hunspell dir so enchant's *.dic
# enumeration does not advertise pattern files as dictionaries —
# they get their own <root>/hyphen either way
dictsrcs="/usr/share/hunspell /usr/local/share/hunspell
/usr/share/myspell /usr/share/myspell/dicts /usr/local/share/myspell"
copy_dicts "$outdir/hunspell" spell $dictsrcs
copy_dicts "$outdir/hyphen" hyph \
	/usr/share/hyphen /usr/local/share/hyphen $dictsrcs
if [ ! -f "$outdir/hunspell/en_US.dic" ] && \
   [ ! -f "$outdir/hunspell/en_GB.dic" ]; then
	echo "linux-bundle: WARNING no English hunspell dictionary" >&2
	echo "  staged — bundled spellcheck needs the hunspell-en-us" >&2
	echo "  package (or equivalent) on the build host" >&2
fi
[ -z "$(find "$outdir/hyphen" -name 'hyph_*.dic' -print -quit)" ] && \
	echo "linux-bundle: WARNING no hyphenation patterns staged" \
		"(libhyphen-dev / hyphen-en package)" >&2

# --------------------------------------------------- GTK4 modules
# immodules / media / printbackends dirs, incl. their giomodule.cache
# files (the caches list bare module names — already relocatable).
# The app exports GTK_PATH=<root>/lib/gtk-4.0 at startup.
if [ -n "$gtkdir" ]; then
	relgtk=$(echo "$gtkdir" | sed 's|.*/\(gtk-4.0/.*\)|\1|')
	mkdir -p "$outdir/lib/$relgtk"
	for sub in "$gtkdir"/*; do
		[ -d "$sub" ] || continue
		cp -aL "$sub" "$outdir/lib/$relgtk/"
	done
else
	echo "linux-bundle: WARNING no gtk-4.0 module dir found" >&2
fi

# ---------------------------------------------------- GStreamer
# plugins for GTK4's libmedia-gstreamer.so; the app exports
# GST_PLUGIN_SYSTEM_PATH_1_0=<root>/lib/gstreamer-1.0 at startup.
if [ -n "$gstdir" ]; then
	mkdir -p "$outdir/lib/gstreamer-1.0"
	gst_plugins | while IFS= read -r gst; do
		cp -aL "$gst" "$outdir/lib/gstreamer-1.0/" 2>/dev/null || true
	done
else
	echo "linux-bundle: WARNING no gstreamer-1.0 plugins dir found" >&2
fi

# ------------------------------------------------ gschemas.compiled
gdir=/usr/share/glib-2.0/schemas
if [ -d "$gdir" ] && command -v glib-compile-schemas >/dev/null 2>&1; then
	bschema="$outdir/share/glib-2.0/schemas"
	mkdir -p "$bschema"
	# all *.xml — the enums live in *.enums.xml and *.gschema.xml
	# reference them; distro *.gschema.override files stay out (they
	# would drag host-specific defaults into the bundle)
	cp -a "$gdir"/*.xml "$bschema/" 2>/dev/null || true
	glib-compile-schemas --strict --targetdir="$bschema" "$bschema" 2>/dev/null ||
		glib-compile-schemas --targetdir="$bschema" "$bschema"
	rm -f "$bschema"/*.xml
else
	echo "linux-bundle: WARNING glib schemas/compiler missing" >&2
fi

# ------------------------------------------------------------- rpath
# runpath = $ORIGIN/<ups-to-bundle-root>/lib — resolves to bundle/lib
# whether the ELF sits in bin/, lib/, or a nested module dir.
set_rpath() { # $1 = file
	dir=$(dirname "$1")
	rel=${dir#"$outdir"}
	rel=${rel#/}
	ups=$(echo "$rel" | tr -cd '/' | wc -c)
	rp='$ORIGIN'
	i=0
	while [ $i -le $ups ]; do rp="$rp/.."; i=$((i+1)); done
	"$patchelf" --set-rpath "$rp/lib" "$1"
}

is_elf() {
	[ "$(od -An -tx1 -N4 "$1" 2>/dev/null | tr -d ' \n')" = "7f454c46" ]
}

find "$outdir" -type f \( -name 'abinova' -o -name '*.so*' \) | \
while read -r f; do
	is_elf "$f" || continue
	set_rpath "$f" || exit 1
done

# ------------------------------------------------------------- strip
if [ $strip -eq 1 ]; then
	find "$outdir" -type f \( -name 'abinova' -o -name '*.so*' \) | \
	while read -r f; do
		is_elf "$f" || continue
		strip --strip-unneeded "$f" 2>/dev/null || true
	done
fi

# -------------------------------------------------------- BUNDLE-INFO
{
	echo "Abinova $version self-contained Linux bundle ($arch)"
	echo "built $(date -u '+%Y-%m-%d %H:%M UTC') on $(. /etc/os-release 2>/dev/null && echo "$PRETTY_NAME" || uname -srm)"
	echo
	echo "entry point: bin/abinova  (RUNPATH \$ORIGIN/../lib)"
	echo "bundled libraries:"
	find "$outdir/lib" -name '*.so*' -type f | sed "s|$outdir/|  |" | sort
	echo
	echo "system (not bundled, per dist/BUNDLED-DEPS.md):"
	echo "$all_libs" | while read -r name path; do
		if is_system_lib "$name"; then echo "  $name -> $path"; fi
	done
	echo
	echo "runtime modules (PACK06):"
	echo "  lib/enchant-2/            spellcheck backends (self-loaded by"
	echo "                            the app when the enchant broker's"
	echo "                            compiled-in provider dir is absent)"
	[ -n "$mlib" ] && [ "$mlib" != "lib" ] && [ -d "$outdir/$mlib/enchant-2" ] && \
		echo "  $mlib/enchant-2/  mirror for relocatable enchant builds"
	echo "  lib/gio/modules/          GIO modules incl. TLS backend"
	echo "                            (GIO_EXTRA_MODULES set by the app)"
	echo "  lib/gtk-4.0/<ver>/        immodules, media, printbackends"
	echo "                            (GTK_PATH set by the app)"
	echo "  lib/gstreamer-1.0/        plugins for the GTK4 media module"
	echo "                            (GST_PLUGIN_SYSTEM_PATH_1_0 set by app)"
	echo "  certs/ca-certificates.crt CA store for the update check"
	echo "                            (GTlsFileDatabase per connection)"
	echo
	echo "data files (PACK07):"
	echo "  hunspell/                   spelling dictionaries — app"
	echo "                              appends the bundle root to"
	echo "                              XDG_DATA_DIRS so enchant's"
	echo "                              hunspell provider, the grammar"
	echo "                              checker and ut_hyphen all see"
	echo "                              them"
	echo "  hyphen/                     hyphenation patterns (hyph_*.dic)"
	echo "  artwork/ fonts/             galleries + bundled font set"
	echo "                              (system fontconfig used — Linux"
	echo "                              ships no private fonts.conf)"
	echo "  (no locale files exist — the UI is English-only, strings"
	echo "   are compiled in)"
} > "$outdir/BUNDLE-INFO.txt"

# ------------------------------------------------------------ verify
if [ $verify -eq 1 ]; then
	echo "linux-bundle: verifying RUNPATH + resolution"
	bad=0
	rp=$("$patchelf" --print-rpath "$outdir/bin/abinova")
	case "$rp" in
	*'../lib'*) : ;;
	*) echo "  FAIL: bin/abinova RUNPATH is '$rp'" >&2; bad=1 ;;
	esac

	tmp=$(mktemp -d)
	env -u LD_LIBRARY_PATH LC_ALL=C ldd "$outdir/bin/abinova" > "$tmp/ldd.out"
	if grep -n "not found" "$tmp/ldd.out"; then bad=1; fi
	# every resolved path must live under the bundle or be a system lib
	awk '/=>/ {print $1, $3}' "$tmp/ldd.out" | while read -r name path; do
		case "$path" in
		"$outdir"/*) : ;;
		*)
			if is_system_lib "$name"; then :; else
				echo "  FAIL: $name resolves outside bundle -> $path" >&2
				echo "leak" >> "$tmp/leaks"
			fi ;;
		esac
	done
	[ -f "$tmp/leaks" ] && bad=1

	# PACK06: the runtime-loaded modules (dlopen'd — invisible to the
	# binary's own ldd) get the same resolution gate
	find "$outdir/lib" -type f -name '*.so*' | while read -r f; do
		is_elf "$f" || continue
		LC_ALL=C ldd "$f" 2>/dev/null | awk -v f="$f" -v out="$outdir" \
			-v leaks="$tmp/leaks" '
			/=> *not found/ {
				print "  FAIL: " f ": " $1 " not found" > "/dev/stderr";
				print "x" >> leaks; next }
			/=>/ {
				p = $3
				if (p ~ "^" out "/") next
				split($1, a, ".")
				# system-soname check duplicated from is_system_lib()
				if ($1 ~ /^(libc|libm|libdl|libpthread|librt|libutil|libresolv|libnsl|libanl|ld-linux-|ld-musl-|linux-vdso|linux-gate)/) next
				print "  FAIL: " f ": " $1 " -> " p " (outside bundle)" \
					> "/dev/stderr"
				print "x" >> leaks
			}'
	done
	[ -f "$tmp/leaks" ] && bad=1

	if [ $bad -eq 0 ] && [ -f "$top/test/wp/BillOfRights.abw" ]; then
		env -u LD_LIBRARY_PATH "$outdir/bin/abinova" \
			--to=pdf --to-name="$tmp/bundle.pdf" \
			"$top/test/wp/BillOfRights.abw" >/dev/null 2>&1 || bad=1
		head -c4 "$tmp/bundle.pdf" 2>/dev/null | grep -q '%PDF' || bad=1
		[ $bad -eq 0 ] && echo "  headless convert: OK"
	fi

	if [ $bad -eq 0 ]; then
		echo "linux-bundle: ldd resolves only bundled+system paths — PASS"
	else
		echo "linux-bundle: verification FAILED (see above)" >&2
		rm -rf "$tmp"
		exit 1
	fi

	# clean-container self-test — needs docker/podman (absent on a
	# plain dev box; the bundle stays verified via the ldd gate above)
	dockerbin=""
	for d in docker podman; do
		command -v "$d" >/dev/null 2>&1 && dockerbin=$d && break
	done
	if [ -n "$dockerbin" ] && [ "$selftest_docker" != "no" ]; then
		echo "linux-bundle: docker self-test (debian:stable-slim)"
		"$dockerbin" run --rm -v "$outdir:/opt/abinova:ro" \
			debian:stable-slim sh -c '
				unset LD_LIBRARY_PATH
				ldd /opt/abinova/bin/abinova | grep -v "not found" >/dev/null &&
				/opt/abinova/bin/abinova --version' || {
			echo "linux-bundle: container self-test FAILED" >&2; exit 1; }
	else
		echo "linux-bundle: no docker/podman — in-build clean-container"
		echo "  check skipped. The full matrix in dist/bundle-verify.sh"
		echo "  runs without docker (OCI rootfs pull + bwrap):"
		echo "    dist/bundle-verify.sh --bundle '$outdir'"
		echo "  or on a docker host:"
		echo "    docker run --rm -v '$outdir':/opt/abinova:ro \\"
		echo "      debian:stable-slim /opt/abinova/bin/abinova --version"
	fi
	rm -rf "$tmp"
fi

cat <<EOF

Bundle ready: $outdir
  run:   $outdir/bin/abinova
  size:  $(du -sh "$outdir" 2>/dev/null | cut -f1)
EOF
