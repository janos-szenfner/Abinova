#!/bin/sh
# sign-windows.sh — Authenticode-sign a Windows bundle (or installer).
#
# Usage:
#   dist/sign-windows.sh <bundle-dir | file> [more files/dirs...]
#
# Signs every PE file (*.exe/*.dll, MZ magic) found under the given
# directories, plus any files listed explicitly, with signtool.exe.
# Run it inside MSYS2 (or on Windows) AFTER assembling the bundle
# with tools/build-windows-msys2.sh --bundle, and again on the
# installer produced by dist/abinova-setup.nsi (the NSIS script can
# also sign itself at build time — see -DSIGNCMD in its header).
#
# Configuration is via environment so certificate material never
# lands in the repo or in CI logs:
#
#   ABINOVA_SIGNTOOL          signtool binary; default: signtool.exe,
#                             then signtool on PATH.  With the Windows
#                             SDK installed signtool lives under
#                             "C:\Program Files (x86)\Windows Kits\10\
#                             bin\<ver>\x64\signtool.exe".
#   ABINOVA_SIGN_SHA1         thumbprint of a cert in the user's/machine
#                             certificate store (recommended for tokens
#                             and EV certs that cannot export a PFX)
#   OR
#   ABINOVA_SIGN_PFX          path to a PFX/PKCS#12 file
#   ABINOVA_SIGN_PFX_PASSWORD PFX password (omit for an empty one; it is
#                             passed to signtool as /p — prefer the
#                             store route on shared machines)
#   ABINOVA_SIGN_TSA          RFC3161 timestamp authority;
#                             default http://timestamp.digicert.com
#
# Dual SHA256 digest + timestamp is applied (/fd sha256 /td sha256).
# After signing, every file is checked with `signtool verify /pa`.
#
# Exit status: 0 when everything configured signed+verified; 1 when a
# prerequisite (signtool, credentials) is missing or any file fails —
# unsigned release artifacts must not look successful.
# See dist/SIGNING.md for certificate acquisition notes.

set -e

signtool=${ABINOVA_SIGNTOOL:-}
if [ -z "$signtool" ]; then
	for c in signtool.exe signtool; do
		command -v "$c" >/dev/null 2>&1 && signtool=$c && break
	done
fi
[ -n "$signtool" ] || {
	echo "sign-windows: signtool not found — install the Windows SDK" >&2
	echo "  (or set ABINOVA_SIGNTOOL)" >&2
	exit 1; }

sha1=${ABINOVA_SIGN_SHA1:-}
pfx=${ABINOVA_SIGN_PFX:-}
pfxpw=${ABINOVA_SIGN_PFX_PASSWORD:-}
tsa=${ABINOVA_SIGN_TSA:-http://timestamp.digicert.com}

if [ -z "$sha1" ] && [ -z "$pfx" ]; then
	cat >&2 <<'EOF'
sign-windows: no certificate configured — nothing signed.
  Pick ONE credential form and re-run:
    store cert:  ABINOVA_SIGN_SHA1=<thumbprint>
    PFX file:    ABINOVA_SIGN_PFX=<file> [ABINOVA_SIGN_PFX_PASSWORD=...]
  See dist/SIGNING.md.
EOF
	exit 1
fi

[ -n "$pfx" ] && [ ! -f "$pfx" ] && {
	echo "sign-windows: ABINOVA_SIGN_PFX '$pfx' not found" >&2
	exit 1; }

[ $# -gt 0 ] || { echo "sign-windows: no files/dirs given" >&2; exit 1; }

is_pe() { # $1 = file; PE images start with 'MZ'
	case "$(od -An -tx1 -N2 "$1" 2>/dev/null | tr -d ' \n')" in
	4d5a) return 0 ;;
	esac
	return 1
}

# collect the target list: every PE under dir args + every file arg
listfile=$(mktemp "${TMPDIR:-/tmp}/abinova-sign.XXXXXX")
trap 'rm -f "$listfile"' EXIT
for a in "$@"; do
	if [ -d "$a" ]; then
		find "$a" -type f \( -iname '*.exe' -o -iname '*.dll' \)
	elif [ -f "$a" ] && is_pe "$a"; then
		echo "$a"
	elif [ -f "$a" ]; then
		echo "sign-windows: skipping $a (not a PE image)" >&2
	else
		echo "sign-windows: skipping $a (not found)" >&2
	fi
done > "$listfile"

if [ ! -s "$listfile" ]; then
	echo "sign-windows: no PE files found under the given paths" >&2
	exit 1
fi

set --
[ -n "$sha1" ] && set -- "$@" /sha1 "$sha1"
if [ -n "$pfx" ]; then
	set -- "$@" /f "$pfx"
	[ -n "$pfxpw" ] && set -- "$@" /p "$pfxpw"
fi

fails=0
count=0
while IFS= read -r f; do
	count=$((count+1))
	"$signtool" sign /fd sha256 /td sha256 /tr "$tsa" \
		/d "Abinova" "$@" "$f" || {
		echo "sign-windows: FAILED to sign $f" >&2
		fails=$((fails+1))
		continue; }
	"$signtool" verify /pa "$f" >/dev/null 2>&1 || {
		echo "sign-windows: FAILED to verify $f" >&2
		fails=$((fails+1)); }
done < "$listfile"

if [ $fails -gt 0 ]; then
	echo "sign-windows: $fails of $count files failed" >&2
	exit 1
fi
echo "sign-windows: $count PE files signed + verified"
