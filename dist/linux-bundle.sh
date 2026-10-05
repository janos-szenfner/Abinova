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
#   lib/gdk-pixbuf-2.0/<ver>/loaders.cache
#   share/glib-2.0/schemas/gschemas.compiled
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
# Enchant backends, GIO modules (libgiognutls + CA certs), GTK4
# print/im/media modules and GStreamer are intentionally NOT bundled
# here — that is PACK06's runtime-module layer. This script ships the
# pieces PACK03 owns: shared-lib closure, rpath, schemas, pixbuf
# loaders+cache.
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

all_libs=$(ldd_libs "$libsrc"/libabinova-*.so \
	${pixdir:+"$pixdir"/*.so} 2>/dev/null)
notfound=$(echo "$all_libs" | awk '/^NOTFOUND/ {print $2}')
if [ -n "$notfound" ]; then
	echo "linux-bundle: unresolved deps before bundling:" >&2
	echo "$notfound" >&2
	exit 1
fi

if [ $print_libs -eq 1 ]; then
	echo "linux-bundle: closure for libabinova-$series.so + pixbuf loaders"
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
	echo "runtime modules intentionally NOT bundled (PACK06):"
	echo "  enchant-2 backends, GIO modules (libgiognutls TLS + CA certs),"
	echo "  GTK4 print/im/media modules, GStreamer"
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
		echo "linux-bundle: no docker/podman — clean-container check"
		echo "  skipped (needs:tool:docker). On a docker host run:"
		echo "  docker run --rm -v '$outdir':/opt/abinova:ro \\"
		echo "    debian:stable-slim /opt/abinova/bin/abinova --version"
	fi
	rm -rf "$tmp"
fi

cat <<EOF

Bundle ready: $outdir
  run:   $outdir/bin/abinova
  size:  $(du -sh "$outdir" 2>/dev/null | cut -f1)
EOF
