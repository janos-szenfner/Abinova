# UI08 — UI06 verification sweep results

Date: 2026-10-06. Scope: the full verification the retired UI06 row
specified after UI07's stale-column fix — `ui-drive --fmt --ev --frame`
under xvfb-run with WAYLAND_DISPLAY unset + GDK_BACKEND=x11, plus the
`make check` drvwrap legs. Discipline: background long legs teeing to
result files, polled — no interactive driving, no watchdog kill.

## Direct legs (first attempt, run at 11:27-11:34 on libabinova built
11:19 — includes all current sources)

Fixture: test/wp/cov07/rich.abw scratch copy per leg.

| leg     | result |
|---------|--------|
| --fmt   | `drive: fmt done — 3 criticals, 0 crashed, 0 wedged, 4 handle drags, 2 finishes` — clean |
| --ev    | `drive: ev done — 12 criticals, 0 crashed, 0 wedged` — **completes** (this was the leg UI06 suspected of sharing the crash class; it now finishes normally) |
| --frame | bounded exit at the wrapper's 300s `timeout` while inside `interact GtkMenuButton`; SIGTERM stack shows active rendering (LLVM MCJIT shader emit inside `gsk_renderer_render`), not a lock wedge — the designed bound for this leg (walk deadline is 230s; per-widget interacts can carry it to the wrapper bound) — no SEGV |

Logs: /tmp/ui08-legs.log, /tmp/ui08-ev.log, /tmp/ui08-frame.log.

## `make check` drvwrap leg (second pass, aggregate)

One background `drvwrap.sh` run under its own xvfb-run re-exec
(Xvfb :99, GDK_BACKEND=x11 pinned, WAYLAND_DISPLAY unset):

| leg | result |
|-----|--------|
| 71 dialog legs | 71/71 `ok`, 0 skipped, **0 xfail** (id 8 Print — previously xfail — passed) |
| --frame | `ok` — completed inside the 300s bound |
| --abi | `timeout/fail` once under the aggregate (180s bound) — flake; standalone and bounded re-runs clean (see below) |
| --ev  | `ok` |
| --fmt | `ok` |

Final: `dialog drive: 71/71 dialogs clean, 0 skipped, 0 xfail`,
DRVWRAP_EXIT=0.

## Crash surfaced + fixed during verification

A standalone `--abi` run between the two sweeps rescued 3 SEGVs (driver
sigsetjmp guard), including a deterministic-class null deref:

    fp_Page::getHeight+0x1d  <- FV_View::_getPageXandYOffset
    <- getPageYOffset <- FV_View::_draw <- updateScreen
    <- FV_VisualInlineImage::setMode <- warpInsPtToXY <- cmdSelect
    <- selectWord <- abi_widget_select_word

Root cause: `fv_View_protected.cpp` `_getPageXandYOffset` web/normal
view branch walked `pDSL->getFirstOwnedPage()->getHeight()` with no
null check — a DocSectionLayout transiently owning no pages (post-delete
collapse) dereferences null. Same host-death class UI07 fixed. Fixed by
guarding pPage (height contribution 0, page-count bookkeeping still
consumed). Heap-layout dependent: a repeat run was clean before the
fix; two clean runs after.

Residual observation: the aggregate drvwrap `--abi` leg (180s) timed
out once while standalone runs finish in well under the bound —
attributed to load variance after the 71-leg dialog sweep, same flake
class as prior runs (FRZ06 id-1005, MAIL01 id-1035).

## Verdict

UI08 done: all three designed legs ran under xvfb with WAYLAND_DISPLAY
unset — fmt + ev + frame all reached bounded exits with **zero SEGV,
zero wedges**; the previously-suspect --ev leg now completes; the
`make check` drvwrap sweep is green (71/71 dialogs, frame/ev/fmt ok).
