#!/bin/sh
# sign-macos.sh — codesign (and optionally notarize) Abinova.app.
#
# Usage:
#   dist/sign-macos.sh [--identity NAME] [--notarize-profile PROFILE]
#                      [--no-sign] <Abinova.app>
#
# Default (no --identity): AD-HOC signing — `codesign -s -` on every
# Mach-O inside-out, then the .app itself.  Ad-hoc signing is the
# minimum that lets an arm64 bundle launch at all (Apple Silicon
# refuses to run unsigned Mach-O), but it is NOT a Gatekeeper pass:
# downloaded copies still get the "damaged/can't be opened" quarantine
# prompt.
#
# Release signing: pass a Developer ID identity (installed in the
# login keychain of an Apple-Developer-account machine):
#
#   dist/sign-macos.sh --identity "Developer ID Application: NAME (TEAMID)" \
#     Abinova.app
#
#   or export ABINOVA_CODESIGN_IDENTITY="Developer ID Application: ..."
#
# Real signing adds --options runtime (hardened runtime — mandatory
# for notarization; no special entitlements are requested) and
# --timestamp (secure timestamp server).  The app is then verified
# with `codesign --verify --deep --strict`.
#
# Notarization (--notarize-profile or ABINOVA_NOTARY_PROFILE): packs
# the .app into a zip with ditto and submits it to Apple via
# `xcrun notarytool submit --wait`, then staples the ticket with
# `xcrun stapler staple`.  The profile is created once per machine:
#
#   xcrun notarytool store-credentials "abinova-notary" \
#     --apple-id <you@example.com> --team-id <TEAMID> \
#     --password <app-specific-password>
#
# Notarization REQUIRES a real Developer-ID identity — Apple rejects
# ad-hoc-signed submissions, so the script refuses the combination.
#
# Runs only on macOS (codesign/xcrun/ditto are Xcode-CLT tools).
# Exit status: 0 on success, 1 on any failure.

set -e

identity=${ABINOVA_CODESIGN_IDENTITY:-}
notary=${ABINOVA_NOTARY_PROFILE:-}
sign=1
appdir=""

while [ $# -gt 0 ]; do
	case "$1" in
	--identity)          identity=$2; shift 2 ;;
	--identity=*)        identity=${1#*=}; shift ;;
	--notarize-profile)  notary=$2; shift 2 ;;
	--notarize-profile=*) notary=${1#*=}; shift ;;
	--no-sign)           sign=0; shift ;;
	-h|--help)           sed -n '2,42p' "$0"; exit 0 ;;
	-*)  echo "sign-macos: unknown option $1" >&2; exit 1 ;;
	*)
		if [ -n "$appdir" ]; then
			echo "sign-macos: multiple app paths given" >&2; exit 1
		fi
		appdir=$1; shift ;;
	esac
done

[ -n "$appdir" ] || { echo "sign-macos: no .app path given" >&2; exit 1; }
[ -d "$appdir" ] || {
	echo "sign-macos: $appdir is not a directory" >&2; exit 1; }

if [ $sign -eq 0 ]; then
	echo "sign-macos: --no-sign, leaving $appdir unsigned" >&2
	exit 0
fi

if ! command -v codesign >/dev/null 2>&1; then
	echo "sign-macos: codesign not found (needs macOS + Xcode CLT:" >&2
	echo "  xcode-select --install).  The bundle will not launch on" >&2
	echo "  Apple Silicon unsigned." >&2
	exit 1
fi

is_macho() {
	case "$(od -An -tx1 -N4 "$1" 2>/dev/null | tr -d ' \n')" in
	cafebabe|cafebabf|bebafeca|bebafecb|cffaedfe|cecfaedf|feedfacf|feedface)
		return 0 ;;
	esac
	return 1
}

# codesign arguments for a single file: identity signing adds the
# hardened runtime and a secure timestamp, both required for a
# notarization submission to be accepted.
cargs=""
if [ -n "$identity" ]; then
	cargs="--options runtime --timestamp"
fi

# Sign every Mach-O inside-out, deepest path first so nested helpers
# seal before their containers; the .app bundle seals last.
find "$appdir/Contents" -type f 2>/dev/null | \
	awk '{ print gsub(/\//,"/"), $0 }' | sort -rn | cut -d' ' -f2- | \
while IFS= read -r f; do
	is_macho "$f" || continue
	if [ -n "$identity" ]; then
		# shellcheck disable=SC2086
		codesign --sign "$identity" $cargs --force "$f"
	else
		codesign --sign - --force "$f" 2>/dev/null || true
	fi
done

if [ -n "$identity" ]; then
	# shellcheck disable=SC2086
	codesign --sign "$identity" $cargs --force "$appdir"
	echo "sign-macos: signed with '$identity' (hardened runtime)"
	codesign --verify --deep --strict --verbose=1 "$appdir"
	echo "sign-macos: codesign --verify --deep --strict PASS"
else
	codesign --sign - --force "$appdir" 2>/dev/null || true
	echo "sign-macos: ad-hoc signed (set ABINOVA_CODESIGN_IDENTITY or"
	echo "  pass --identity for a Developer-ID release signature)"
fi

# ------------------------------------------------------------ notarize
if [ -n "$notary" ]; then
	if [ -z "$identity" ]; then
		echo "sign-macos: refusing to notarize an ad-hoc-signed app —" >&2
		echo "  Apple rejects it; pass --identity first" >&2
		exit 1
	fi
	for t in ditto xcrun; do
		command -v "$t" >/dev/null 2>&1 || {
			echo "sign-macos: $t not found — cannot notarize" >&2
			exit 1; }
	done
	pkg="$appdir.zip"
	rm -f "$pkg"
	ditto -c -k --keepParent "$appdir" "$pkg"
	echo "sign-macos: submitting to notarytool (profile '$notary')…"
	xcrun notarytool submit "$pkg" --keychain-profile "$notary" --wait
	xcrun stapler staple "$appdir"
	rm -f "$pkg"
	xcrun stapler validate "$appdir" >/dev/null 2>&1 || true
	echo "sign-macos: notarized + stapled"
fi
