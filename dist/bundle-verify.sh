#!/bin/sh
# bundle-verify.sh — per-OS smoke matrix for the packed bundle output.
#
# What it asserts per leg:
#   * the bundle binary exists and starts (abinova --version)
#   * a test .docx converts to a valid PDF headlessly inside the clean
#     environment (%PDF magic, non-empty)
#   * the shared-library closure resolves ONLY into the bundle — every
#     resolved path must live under the bundle mount or carry a
#     libc-family soname (the declared "system" set, see
#     linux-bundle.sh is_system_lib / dist/BUNDLED-DEPS.md)
#
# Linux legs run the bundle in a REAL clean container per distro:
#   debian  -> docker.io image library/debian:stable-slim
#   ubuntu  -> docker.io image library/ubuntu:24.04
#   suse    -> docker.io image opensuse/leap:15.6
# Backend order (--backend): docker or podman when installed; otherwise
# the script anonymously pulls the same image's rootfs straight off the
# Docker Hub OCI registry API (curl+tar only — no docker daemon needed)
# and runs it under bubblewrap (bwrap).  The host's /usr is invisible
# inside either backend, so a missing bundled library or runtime module
# cannot be masked by the host system.
#
# Usage:
#   dist/bundle-verify.sh [--bundle DIR] [--os LIST] [--fixture PATH]
#                         [--backend auto|docker|podman|bwrap]
#                         [--cache DIR] [--refresh] [-h|--help]
#
#   --os LIST   comma list of legs.  Linux legs: debian,ubuntu,suse
#               (or the alias 'linux' = all three).  Documented legs:
#               macos,windows,freebsd — printed runner steps on foreign
#               hosts, real checks when run on the matching platform.
#               'all' = linux + the three documented legs.
#   --fixture   .docx used for the convert leg (default:
#               fuzz/corpus/docx/seed_gettysburg.docx)
#   --cache     image/rootfs cache (default: dist/.bundle-verify-cache)
#   --refresh   re-pull distro rootfs even when cached
#
# Exit status: 0 when every selected leg passes; 1 on any failure.
# Documented legs skipped on a foreign platform do not fail the run.
#
# ----------------------------------------------------------------------
# NON-LINUX RUNNER LEGS (these are also printed by --os all)
#
# macOS  (needs:macos — MUST be an arm64 machine, e.g. GitHub macos-14+;
#        x86_64 CI would miss the unsigned-char/alignment issues PORT08
#        hardened; ad-hoc codesign by build-macos.sh is enough to launch,
#        real notarization is PACK09):
#     tools/build-macos.sh --bundle
#     dist/bundle-verify.sh --os macos --bundle dist/Abinova.app
#   on Darwin this leg runs for real: otool -L gate (every dep must be
#   @executable_path/../Frameworks or /usr/lib|/System), --version, and
#   the docx->pdf convert.
#
# Windows (needs:windows — MSYS2 UCRT64):
#     tools/build-windows-msys2.sh --bundle
#     dist/bundle-verify.sh --os windows --bundle dist/abinova-<ver>-windows-<arch>
#   under MSYS2 this leg runs for real: exe+manifest exist, exe starts,
#   objdump -p DLL names all resolve inside the bundle or the system
#   allowlist.
#
# FreeBSD (needs:freebsd — no bundle format; the pkg dep set IS the
#        contract.  Deps land in /usr/local not /usr — PKG_CONFIG_PATH
#        must include /usr/local/libdata/pkgconfig; getentropy/
#        arc4random_buf cover the PORT03 RNG; gtk4-unix-print is
#        available for printing; /proc is absent by default, so nothing
#        may read /proc paths):
#     tools/build-freebsd.sh
#     dist/bundle-verify.sh --os freebsd
#   on FreeBSD this leg runs the built src/abinova + an ldd 'not found'
#   check.
#
# Suggested CI matrix (each leg in a job on a matching runner):
#   linux:   dist/bundle-verify.sh --os linux          (any runner —
#            docker if present, else bwrap+OCI pull)
#   macos:   runs-on: macos-14  (arm64) -> build-macos.sh --bundle +
#            --os macos
#   windows: msys2 shell -> build-windows-msys2.sh --bundle + --os windows
#   freebsd: vmaction/freebsd-vm -> build-freebsd.sh + --os freebsd
#
# Tested on: Linux x86-64 (bwrap backend exercised end-to-end).

set -u

bundle=""
oslist="linux"
fixture=""
backend="auto"
cache=""
refresh=0

while [ $# -gt 0 ]; do
	case "$1" in
	--bundle)     bundle=$2; shift 2 ;;
	--bundle=*)   bundle=${1#*=}; shift ;;
	--os)         oslist=$2; shift 2 ;;
	--os=*)       oslist=${1#*=}; shift ;;
	--fixture)    fixture=$2; shift 2 ;;
	--fixture=*)  fixture=${1#*=}; shift ;;
	--backend)    backend=$2; shift 2 ;;
	--backend=*)  backend=${1#*=}; shift ;;
	--cache)      cache=$2; shift 2 ;;
	--cache=*)    cache=${1#*=}; shift ;;
	--refresh)    refresh=1; shift ;;
	-h|--help)    sed -n '2,81p' "$0"; exit 0 ;;
	*) echo "bundle-verify: unknown option $1" >&2; exit 1 ;;
	esac
done

top=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$top"

uname_s=$(uname -s)
uname_m=$(uname -m)
# docker/OCI arch spelling
case $uname_m in
x86_64|amd64) oci_arch=amd64 ;;
aarch64|arm64) oci_arch=arm64 ;;
*) oci_arch=$uname_m ;;
esac

vmaj=$(sed -n 's/^m4_define(\[abi_version_major\], \[\(.*\)\])/\1/p' configure.ac)
vmin=$(sed -n 's/^m4_define(\[abi_version_minor\], \[\(.*\)\])/\1/p' configure.ac)
vmic=$(sed -n 's/^m4_define(\[abi_version_micro\], \[\(.*\)\])/\1/p' configure.ac)
version=$vmaj.$vmin.$vmic

[ -n "$bundle" ] || bundle="$top/dist/abinova-$version-linux-$uname_m"
[ -n "$cache" ]  || cache="$top/dist/.bundle-verify-cache"
[ -n "$fixture" ] || fixture="$top/fuzz/corpus/docx/seed_gettysburg.docx"

case $backend in auto|docker|podman|bwrap) ;;
*) echo "bundle-verify: bad --backend '$backend'" >&2; exit 1 ;;
esac

# mount points used inside every sandbox — under /tmp so they are
# creatable even on a fully read-only distro rootfs
BUNDLE_MNT=/tmp/bundle
WORK_MNT=/tmp/work

fail_count=0
skip_count=0
pass_count=0

leg_pass()  { pass_count=$((pass_count+1)); echo "== $1: PASS"; }
leg_fail()  { fail_count=$((fail_count+1)); echo "== $1: FAIL ($2)" >&2; }
leg_skip()  { skip_count=$((skip_count+1)); echo "== $1: SKIP ($2)"; }

# -------------------------------------------------------- OCI pull ----
# Anonymous Docker Hub pull: token -> index -> arch manifest -> layers.
# Extracts the image rootfs into $3.  Pure curl+sed+tar — no daemon,
# no python.
oci_pull_rootfs() { # repo tag destdir
	repo=$1 tag=$2 dest=$3
	stamp="$dest/.bundle-verify-image"

	mkdigest=$(curl -sf --max-time 30 \
		"https://auth.docker.io/token?service=registry.docker.io&scope=repository:$repo:pull" \
		| sed -n 's/.*"token":"\([^"]*\)".*/\1/p') || {
		echo "  $repo: registry token request failed" >&2; return 1; }
	[ -n "$mkdigest" ] || { echo "  $repo: empty registry token" >&2; return 1; }
	# variable reuse: mktoken holds the bearer token
	mktoken=$mkdigest

	# hub returns some indexes/manifests pretty-printed — strip all
	# whitespace up front so the field greps see compact JSON
	idx=$(curl -sf --max-time 30 -H "Authorization: Bearer $mktoken" \
		-H "Accept: application/vnd.oci.image.index.v1+json,application/vnd.docker.distribution.manifest.list.v2+json,application/vnd.docker.distribution.manifest.v2+json" \
		"https://registry-1.docker.io/v2/$repo/manifests/$tag" \
		| tr -d ' \t\r\n') || {
		echo "  $repo:$tag index fetch failed" >&2; return 1; }

	case $idx in
	*'"manifests"'*)
		# multi-arch index — pick the linux/$oci_arch entry;
		# digest always precedes the platform object in an entry
		mdig=$(echo "$idx" | sed 's/},{/\n/g' \
			| grep '"architecture":"'"$oci_arch"'"' \
			| grep '"os":"linux"' \
			| sed -n 's/.*"digest":"\(sha256:[0-9a-f]*\)".*/\1/p' \
			| head -n1) ;;
	*)
		# single-arch manifest returned directly
		mdig=$tag ;;
	esac
	[ -n "$mdig" ] || {
		echo "  $repo:$tag: no linux/$oci_arch manifest in index" >&2
		return 1; }

	man=$(curl -sf --max-time 30 -H "Authorization: Bearer $mktoken" \
		-H "Accept: application/vnd.oci.image.manifest.v1+json,application/vnd.docker.distribution.manifest.v2+json" \
		"https://registry-1.docker.io/v2/$repo/manifests/$mdig" \
		| tr -d ' \t\r\n') || {
		echo "  $repo@$mdig manifest fetch failed" >&2; return 1; }
	case $man in *'"layers"'*) : ;;
	*) echo "  $repo: manifest has no layers (schema-1?)" >&2; return 1 ;;
	esac

	if [ -f "$stamp" ] && [ "$refresh" -eq 0 ] &&
		grep -q "$repo:$tag@$mdig" "$stamp"; then
		echo "  rootfs $repo:$tag (cached)"
		return 0
	fi

	layers=$(echo "$man" | sed 's/.*"layers":\[//; s/\].*//' \
		| sed 's/},{/\n/g' \
		| sed -n 's/.*"digest":"\(sha256:[0-9a-f]*\)".*/\1/p')
	[ -n "$layers" ] || { echo "  $repo: manifest lists zero layers" >&2; return 1; }

	rm -rf "$dest"
	mkdir -p "$dest"
	tmpd=$(mktemp -d) || return 1

	n=0
	rc=0
	for ldig in $layers; do
		n=$((n+1))
		echo "  fetching layer $n ($(echo "$ldig" | cut -c8-19))"
		if ! curl -sfL --max-time 300 -H "Authorization: Bearer $mktoken" \
			-o "$tmpd/layer$n.tar" \
			"https://registry-1.docker.io/v2/$repo/blobs/$ldig"; then
			echo "  blob $ldig fetch failed" >&2; rc=1; break
		fi
		# docker v2 layers are gzip'd tar; OCI allows zstd too
		case $(dd if="$tmpd/layer$n.tar" bs=4 count=1 2>/dev/null | od -An -tx1 | tr -d ' ') in
		1f8b*) tar -xzf "$tmpd/layer$n.tar" -C "$dest" || rc=1 ;;
		28b52ffd*) tar --zstd -xf "$tmpd/layer$n.tar" -C "$dest" || rc=1 ;;
		*) tar -xf "$tmpd/layer$n.tar" -C "$dest" || rc=1 ;;
		esac
		rm -f "$tmpd/layer$n.tar"
		[ $rc -eq 0 ] || break
		# apply whiteouts (rare in base images, but correct)
		find "$dest" -name '.wh..wh..opq' | while read -r wh; do
			d=$(dirname "$wh"); rm -rf "$d"/* ; rm -f "$wh"
		done
		find "$dest" -name '.wh.*' | while read -r wh; do
			rm -rf "$(dirname "$wh")/$(basename "$wh" | sed 's/^\.wh\.//')"
			rm -f "$wh"
		done
	done

	rm -rf "$tmpd"
	[ $rc -eq 0 ] || { rm -rf "$dest"; return 1; }
	# refuse to cache an obviously-broken extraction
	[ -d "$dest/bin" ] && [ -d "$dest/lib" ] || {
		echo "  $repo: extracted rootfs has no /bin+/lib" >&2
		rm -rf "$dest"; return 1; }
	echo "$repo:$tag@$mdig" > "$stamp"
	echo "  rootfs $repo:$tag pulled ($(LC_ALL=C du -sh "$dest" | cut -f1))"
	return 0
}

# -------------------------------------------------- in-container leg --
# Written into each leg's work dir; executed INSIDE the clean container
# where nothing from the host is visible except the bundle (read-only)
# and the work dir.
write_leg_script() { # destfile
	cat > "$1" <<LEG_EOF
#!/bin/sh
# PACK08 clean-container leg — runs inside the distro image.
b=$BUNDLE_MNT/bin/abinova
export HOME=$WORK_MNT XDG_CACHE_HOME=$WORK_MNT/.cache \\
	XDG_CONFIG_HOME=$WORK_MNT/.config XDG_DATA_HOME=$WORK_MNT/.local/share \\
	GCOV_PREFIX=$WORK_MNT/gcov GCOV_PREFIX_STRIP=8

[ -x "\$b" ] || { echo "LEG-FAIL: no binary at \$b"; exit 1; }
echo "-- os: \$(sed -n 's/^PRETTY_NAME=//p' /etc/os-release | tr -d '"')"

# 1) binary starts
"\$b" --version || { echo "LEG-FAIL: --version"; exit 1; }

# 2) shared-lib closure — no 'not found', nothing outside the bundle
#    except libc-family sonames
if command -v ldd >/dev/null 2>&1; then
	lc=\$(ldd "\$b" 2>&1)
else
	ld=\$(ls /lib64/ld-*.so* /lib/ld-*.so* /lib/*/ld-*.so* 2>/dev/null | head -n1)
	[ -n "\$ld" ] && lc=\$("\$ld" --list "\$b" 2>&1)
fi
[ -n "\${lc:-}" ] || { echo "LEG-FAIL: no ldd/ld.so available"; exit 1; }
echo "\$lc" | grep "not found" && { echo "LEG-FAIL: unresolved deps"; exit 1; }
bad=""
echo "\$lc" | awk '/=>/{print \$1 "|" \$3}' | while read -r line; do
	s=\${line%%|*}; p=\${line#*|}
	case \$p in $BUNDLE_MNT/*) continue ;; esac
	case \$s in
	libc.so*|libm.so*|libmvec.so*|libdl.so*|libpthread.so*|librt.so*|\\
	libutil.so*|libresolv.so*|libnsl.so*|libanl.so*|ld-linux*|ld64*|\\
	ld-musl*|linux-vdso*|linux-gate*|/dev/null*) continue ;;
	esac
	echo "\$s -> \$p"
done > $WORK_MNT/outside.txt
if [ -s $WORK_MNT/outside.txt ]; then
	echo "LEG-FAIL: deps resolved outside bundle:" >&2
	cat $WORK_MNT/outside.txt >&2
	exit 1
fi
echo "-- ldd: closure = bundle + libc family only"

# 3) docx -> pdf inside the container
"\$b" --to=pdf --to-name=$WORK_MNT/out.pdf $WORK_MNT/fixture.docx \\
	>/dev/null 2>&1 || { echo "LEG-FAIL: convert exit"; exit 1; }
magic=\$(dd if=$WORK_MNT/out.pdf bs=4 count=1 2>/dev/null)
[ "\$magic" = "%PDF" ] || { echo "LEG-FAIL: no pdf"; exit 1; }
echo "-- docx->pdf: OK (\$(wc -c < $WORK_MNT/out.pdf) bytes)"
LEG_EOF
	chmod +x "$1"
}

# ---------------------------------------------------- linux leg -------
# image table: leg -> "repo tag"
linux_image() {
	case $1 in
	debian) echo "library/debian stable-slim" ;;
	ubuntu) echo "library/ubuntu 24.04" ;;
	suse)   echo "opensuse/leap 15.6" ;;
	esac
}

run_linux_leg() { # leg
	leg=$1
	set -- $(linux_image "$leg")
	repo=$1 tag=$2

	echo "== $leg: $repo:$tag"

	work="$cache/run-$leg"
	rm -rf "$work"; mkdir -p "$work"
	cp "$fixture" "$work/fixture.docx" || {
		leg_fail "$leg" "fixture copy"; return; }
	write_leg_script "$work/leg.sh"

	runner=""
	case $backend in
	docker|podman) runner=$backend ;;
	bwrap) runner=bwrap ;;
	auto)
		for d in docker podman; do
			command -v "$d" >/dev/null 2>&1 && runner=$d && break
		done
		[ -z "$runner" ] && runner=bwrap ;;
	esac

	if [ "$runner" = bwrap ]; then
		command -v bwrap >/dev/null 2>&1 || {
			leg_fail "$leg" "no docker/podman and no bwrap"; return; }
		rootfs="$cache/rootfs-$leg"
		oci_pull_rootfs "$repo" "$tag" "$rootfs" || {
			leg_fail "$leg" "rootfs pull"; return; }
		bwrap --unshare-all --die-with-parent \
			--ro-bind "$rootfs" / \
			--dev /dev --proc /proc --tmpfs /tmp \
			--ro-bind "$bundle" "$BUNDLE_MNT" \
			--bind "$work" "$WORK_MNT" \
			-- /bin/sh "$WORK_MNT/leg.sh" > "$work/leg.log" 2>&1
	else
		if ! "$runner" image inspect "$repo:$tag" >/dev/null 2>&1; then
			echo "  $runner pull $repo:$tag"
			"$runner" pull "$repo:$tag" > "$work/pull.log" 2>&1 || {
				leg_fail "$leg" "image pull"; return; }
		fi
		"$runner" run --rm --network none \
			-v "$bundle:$BUNDLE_MNT:ro" -v "$work:$WORK_MNT" \
			"$repo:$tag" /bin/sh "$WORK_MNT/leg.sh" \
			> "$work/leg.log" 2>&1
	fi
	rc=$?

	# surface the leg's own progress + gcov-free log
	grep -v '^profiling:' "$work/leg.log" | sed 's/^/    /'
	if [ $rc -eq 0 ] && [ -s "$work/out.pdf" ]; then
		leg_pass "$leg"
	else
		leg_fail "$leg" "rc=$rc — see $work/leg.log"
	fi
}

# ------------------------------------------------- documented legs ----
print_steps() { sed -n '/^# NON-LINUX RUNNER LEGS/,/^# Tested on:/p' "$0" | sed 's/^# \{0,1\}//'; }

leg_macos() {
	if [ "$(uname -s)" != "Darwin" ]; then
		leg_skip macos "needs:macos arm64 runner — steps below"
		print_steps | sed -n '/macOS/,/^$/p'
		return
	fi
	app=${bundle%/}
	exe="$app/Contents/MacOS/abinova-bin"
	[ -x "$exe" ] || exe="$app/Contents/MacOS/abinova"
	[ -x "$exe" ] || { leg_fail macos "no binary in $app"; return; }
	bad=$(otool -L "$exe" | awk 'NR>1{print $1}' | grep -v '^@executable_path/\|^/usr/lib/\|^/System/' || true)
	[ -z "$bad" ] || { leg_fail macos "non-framework deps: $bad"; return; }
	"$exe" --version >/dev/null || { leg_fail macos "--version"; return; }
	w=$(mktemp -d)
	"$exe" --to=pdf --to-name="$w/out.pdf" "$fixture" >/dev/null 2>&1 &&
		head -c4 "$w/out.pdf" | grep -q '%PDF' &&
		leg_pass macos || leg_fail macos "convert"
	rm -rf "$w"
}

leg_windows() {
	case ${MSYSTEM:-}${OS:-} in
	""|*nux*) ;;
	esac
	if [ -z "${MSYSTEM:-}" ]; then
		leg_skip windows "needs:windows — MSYS2 runner steps below"
		print_steps | sed -n '/Windows/,/^$/p'
		return
	fi
	exe="$bundle/bin/abinova.exe"
	[ -x "$exe" ] || { leg_fail windows "no $exe"; return; }
	[ -f "$bundle/bin/abinova.exe.manifest" ] ||
		echo "    warn: no exe manifest sidecar"
	bad=$(objdump -p "$exe" | awk '/DLL Name/{print $3}' | while read -r d; do
		case $d in
		KERNEL32.dll|USER32.dll|GDI32.dll|ADVAPI32.dll|SHELL32.dll|\
		msvcrt.dll|ucrtbase.dll|ole32.dll|WS2_32.dll|CRYPT32.dll|\
		api-ms-win-*|COMCTL32.dll|OLEAUT32.dll|SHLWAPI.dll|\
		IMM32.dll|WINMM.dll|SETUPAPI.dll|VERSION.dll|\
		CRYPTBASE.DLL|BCryptPrimitives.dll|bcrypt.dll) continue ;;
		esac
		find "$bundle" -iname "$d" -print -quit | grep -q . || echo "$d"
	done)
	[ -z "$bad" ] || { leg_fail windows "unresolved dlls: $bad"; return; }
	"$exe" --version >/dev/null || { leg_fail windows "--version"; return; }
	w=$(mktemp -d)
	"$exe" --to=pdf --to-name="$w/out.pdf" "$fixture" >/dev/null 2>&1 &&
		head -c4 "$w/out.pdf" | grep -q '%PDF' &&
		leg_pass windows || leg_fail windows "convert"
	rm -rf "$w"
}

leg_freebsd() {
	if [ "$(uname -s)" != "FreeBSD" ]; then
		leg_skip freebsd "needs:freebsd — runner steps below"
		print_steps | sed -n '/FreeBSD/,/^$/p'
		return
	fi
	bin=""
	[ -f "$bundle/bin/abinova" ] && bin="$bundle/bin/abinova"
	[ -z "$bin" ] && [ -x "$top/src/abinova" ] && bin="$top/src/abinova"
	[ -n "$bin" ] || { leg_fail freebsd "no built binary"; return; }
	ldd "$bin" | grep -q "not found" && { leg_fail freebsd "unresolved libs"; return; }
	"$bin" --version >/dev/null || { leg_fail freebsd "--version"; return; }
	w=$(mktemp -d)
	"$bin" --to=pdf --to-name="$w/out.pdf" "$fixture" >/dev/null 2>&1 &&
		head -c4 "$w/out.pdf" | grep -q '%PDF' &&
		leg_pass freebsd || leg_fail freebsd "convert"
	rm -rf "$w"
}

# ------------------------------------------------------------- run ----
if [ ! -f "$fixture" ]; then
	echo "bundle-verify: fixture $fixture missing" >&2; exit 1
fi

# expand aliases
legs=""
for l in $(echo "$oslist" | tr ',' ' '); do
	case $l in
	linux) legs="$legs debian ubuntu suse" ;;
	all)   legs="$legs debian ubuntu suse macos windows freebsd" ;;
	*)     legs="$legs $l" ;;
	esac
done

echo "bundle-verify: bundle=$bundle"
echo "bundle-verify: legs=$(echo $legs)"

need_bundle=0
for l in $legs; do
	case $l in debian|ubuntu|suse) need_bundle=1 ;; esac
done
if [ $need_bundle -eq 1 ] && [ ! -x "$bundle/bin/abinova" ]; then
	echo "bundle-verify: $bundle/bin/abinova missing —" >&2
	echo "  build it first: dist/linux-bundle.sh" >&2
	exit 1
fi

mkdir -p "$cache"

for l in $legs; do
	case $l in
	debian|ubuntu|suse) run_linux_leg "$l" ;;
	macos)   leg_macos ;;
	windows) leg_windows ;;
	freebsd) leg_freebsd ;;
	*) echo "== $l: unknown leg" >&2; fail_count=$((fail_count+1)) ;;
	esac
done

echo "----------------------------------------"
echo "bundle-verify: $pass_count pass, $fail_count fail, $skip_count documented-skip"
[ $fail_count -eq 0 ]
