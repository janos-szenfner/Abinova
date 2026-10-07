#!/bin/sh
# check-freeze.sh — TST12 freeze gate.  Two halves:
#
#  1. unit time bounds — the Abinova-test cases covering the FRZ01-03
#     fixes, run through the test binary's description-prefix filter:
#     the redraw-notify stall bound, the status-bar urgent no-sleep
#     bound, the content-serial autosave tests, the clipboard payload
#     ownership bound and the XAP_Frame::backup serialize bound.
#  2. ui-drive --freeze — a heartbeat leg that scripts the operations
#     the fixes touched (doc load, burst typing, scroll, autosave
#     ticks, dialogs, an async media embed, a .doc import) while a
#     40ms GSource measures the gap between main-context services;
#     a synchronous stall past the bound fails the leg.
#
# The heartbeat half needs a display: xvfb-run when installed, else
# the live session; without either it reports SKIP and the unit half
# still gates.  FREEZE_NO_DRIVE=1 skips the drive leg entirely.
#
# Env:
#   FREEZE_NO_DRIVE=1            skip the ui-drive heartbeat leg
#   ABINOVA_TEST_LIVE_DISPLAY=1  run on the live display even when
#                                xvfb-run is installed

set -u

SRCROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
TESTDIR=$SRCROOT/src/wp/test
TESTBIN=$TESTDIR/Abinova-test
DRIVE=$TESTDIR/ui-drive
LIBTOOL="$SRCROOT/libtool --mode=execute"
ABINOVA_TEST_SRC_DIR=$SRCROOT
export ABINOVA_TEST_SRC_DIR

fails=0

# ---- unit half -------------------------------------------------------
if [ ! -e "$TESTBIN" ]; then
	echo "check-freeze: SKIP (no built $TESTBIN — run make first)"
	exit 0
fi

echo "check-freeze: unit time bounds"
(cd "$TESTDIR" && $LIBTOOL ./Abinova-test \
	"notifyPieceTableChangeStart does not stall" \
	"PD_Document content serial" \
	"XAP_StatusBar" \
	"XAP_Frame backup timing" \
	"AD_Document basics" \
	"snapshotData hands" \
	"oversized clipboard payload") || {
	echo "check-freeze: unit bounds FAILED" >&2
	fails=$((fails + 1))
}

# ---- drive half ------------------------------------------------------
if [ -n "${FREEZE_NO_DRIVE:-}" ]; then
	echo "check-freeze: drive leg skipped (FREEZE_NO_DRIVE)"
elif [ ! -e "$DRIVE" ]; then
	echo "check-freeze: drive leg SKIP (ui-drive not built)"
else
	runner=
	if [ -z "${ABINOVA_TEST_LIVE_DISPLAY:-}" ] && \
	   command -v xvfb-run >/dev/null 2>&1; then
		# same backend pinning as drvwrap: GTK4's wayland backend
		# still finds the compositor socket when WAYLAND_DISPLAY is
		# only unset, so strip it before xvfb-run
		unset WAYLAND_DISPLAY
		GDK_BACKEND=x11
		export GDK_BACKEND
		runner="xvfb-run -a"
	fi
	if [ -z "$runner" ] && [ -z "${DISPLAY:-}" ] && \
	   [ -z "${WAYLAND_DISPLAY:-}" ]; then
		echo "check-freeze: drive leg SKIP (no usable display)"
	else
		scratch="${TMPDIR:-/tmp}/check-freeze-$$.abw"
		cp "$SRCROOT/test/wp/cov07/rich.abw" "$scratch" || {
			echo "check-freeze: cannot stage scratch doc" >&2
			exit 1
		}
		echo "check-freeze: heartbeat leg"
		(cd "$TESTDIR" && UI_DRIVE_DOC="$scratch" \
			timeout -k 15 240 $runner $LIBTOOL ./ui-drive --freeze)
		rc=$?
		rm -f "$scratch" "${TMPDIR:-/tmp}/ui-drive-freeze-embed.bin"
		case "$rc" in
			0)  echo "check-freeze: heartbeat leg ok" ;;
			77) echo "check-freeze: drive leg SKIP (no interactive context)" ;;
			*)  echo "check-freeze: heartbeat leg FAILED (rc=$rc)" >&2
			    fails=$((fails + 1)) ;;
		esac
	fi
fi

if [ "$fails" -gt 0 ]; then
	echo "check-freeze: FAIL ($fails failing half/halves)" >&2
	exit 1
fi
echo "check-freeze: PASS"
exit 0
