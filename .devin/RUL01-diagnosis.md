# RUL01 diagnosis — GTK4 ruler drag intermittently loses the release

RUL02 deliverable (diagnosis only — **no fix implemented here**).

## Symptom

`ui-drive --ruler` under Xvfb fails **intermittently** (roughly 1 in 3–5
runs) on the off-band drag checks. Two equivalent signatures have been
observed, on *both* rulers:

- `FAIL ruler: left margin off-band delta 0 lu, expected 660..1380`
  (left ruler top-margin drag, vertical motion tracked off-band)
- `FAIL ruler: left indent off-band drag delta 0 lu, expected magnitude 360..4800`
  (top ruler marker drag, horizontal motion tracked off-band)

So this is **not left-ruler-specific** — it is a shared `AP_UnixRuler`
gesture-path race. The RUL01 GtkGestureDrag bridge (f592aaf) fixed the
bulk of the bug; what remains is this flaky release loss.

## Repro recipe

```sh
cd src/wp/test/unix          # ui-drive binary lives in src/wp/test
unset WAYLAND_DISPLAY
for i in 1 2 3 4 5 6; do
  GDK_BACKEND=x11 RULER_TRACE=1 xvfb-run -a ./ui-drive --ruler
done
```

Requires X11 (XTest injection); exits 77 without it. Repeat until a run
prints a FAIL line — the failure is a timing race, ~1-in-3 to 1-in-5.

## Observed event stream

All `fe *` traces are `RULER_TRACE`-gated fprintf instrumentation added
in `ap_UnixRuler.cpp` / `ap_LeftRuler.cpp` (silent by default).

### Passing run (left-ruler top-margin drag)

```
fe drag-end w=0x... off(56,80) seq=(nil) handles=1 seqstate=0 evtype=3 evtime=209309339
leftRuler release y=2664 what=1 valid=1 ignored=0
leftRuler release ygrid=2400 oldY=1440 ctr=2640
leftRuler release APPLY page-margin-top=10pi
ruler: left-ruler margin drag off-band applied at tracked y (delta 960 lu)
```

`drag-end` arrives inside a real `GDK_BUTTON_RELEASE` (evtype=3) with a
real server timestamp; `gtk_gesture_handles_sequence` is still TRUE;
`gtk_gesture_drag_get_start_point()` succeeds; `mouseRelease()` runs and
applies `page-margin-top`.

### Failing run (same drag)

```
fe drag-CANCEL w=0x...9d0 seq=(nil) evtype=1 evpos(73,385) evstate=00000000 evtime=0
fe drag-end w=0x...9d0 off(56,80) seq=(nil) handles=0 seqstate=0 evtype=1 evtime=0
fe drag-end no start point
FAIL ruler: left margin off-band delta 0 lu, expected 660..1380
```

Same on the top ruler when it flakes there:

```
fe drag-CANCEL w=0x...f00 seq=(nil) evtype=1 evpos(393,152) evstate=00000000 evtime=0
fe drag-end w=0x...f00 off(0,0)   seq=(nil) handles=0 seqstate=0 evtype=1 evtime=0
```

The smoking gun is in the canceling event itself:

- `evtype=1` = `GDK_MOTION_NOTIFY`
- `evpos` = the **release position in native/surface coords**
  ((73,385) is exactly where the leg releases the pointer)
- `evstate=0` = **no buttons held**
- `evtime=0` = `GDK_CURRENT_TIME` — a **synthesized** event, not an
  X-originated one (real X events carry a nonzero server timestamp)

## Root cause

A **synthetic, buttonless `GDK_MOTION_NOTIFY`** cancels the drag
mid-flight.

GTK4/GDK synthesizes motion events to refresh pointer focus when a
layout changes under a stationary pointer:

- `gtk_window_native_layout` (gtkwindow.c ~line 2162) calls
  `gdk_surface_request_motion(focus_surface)` whenever the toplevel
  needs allocation — comment in source: "This fake motion event is
  needed for getting up to date pointer focus and coordinates when the
  pointer didn't move but the layout changed within the window."
  (GtkPopover's `maybe_request_motion_event` is the only other caller.)
- That queues `request_motion_cb` (idle, `GDK_PRIORITY_REDRAW + 20`)
  → frame-clock `FLUSH_EVENTS` phase → `gdk_surface_flush_events`
  → `gdk_surface_ensure_motion` (gdksurface.c), which builds
  `gdk_motion_event_new(surface, device, NULL, GDK_CURRENT_TIME,
  state, x, y, NULL)` — timestamp 0, and `state` = the **live
  server-side button mask** at synthesis time.

The race: after `XTestFakeButtonEvent(release)` the X server registers
button-up immediately, but the `ButtonRelease` event sits queued on the
client while main-loop idles/frame-clock phases still run. If
`ensure_motion` fires in that window, the synthetic motion lands at the
release position with `state=0`. The ruler still owns the implicit grab
(the grab is only cleared when the release event is *processed*), so
`handle_pointing_event` routes the motion to the ruler.

`gtk_gesture_single_handle_event` (gtkgesturesingle.c), `MOTION_NOTIFY`
case: the gesture still tracks the sequence, computes `button` from the
event's modifier mask — 0 buttons → `button == 0` →
`gtk_event_controller_reset` → `_gtk_gesture_cancel_all`, which removes
the point and then emits `END` via `check_recognized`.

That produces the observed chain:

1. `cancel` emitted (our `fe drag-CANCEL` trace, current event = the
   synthetic motion).
2. `drag-end` emitted with correct `offset_x/offset_y`
   (GTK keeps `priv->start_x`/`last_x` internally), **but** the sequence
   is already gone — `handles=0`, `seq=(nil)`.
3. `AP_UnixRuler::_fe::drag_end` calls
   `gtk_gesture_drag_get_start_point()` → FALSE (it requires
   `gtk_gesture_single_get_current_sequence()` non-NULL).
4. Handler returns early → `AP_LeftRuler::mouseRelease()` /
   `AP_TopRuler::mouseRelease()` never runs → the drag's final apply
   (margin write / marker move) is dropped → measured delta 0.
5. The real `ButtonRelease` then arrives to a gesture that no longer
   handles the sequence → nothing further fires.

The intermittency is inherent: it depends on a
`request_motion`-scheduled frame flush landing inside the tiny
server-release → client-dispatch window — i.e., on layout/allocation
activity coinciding with release time. That matches the observed flake
rate and why CLOSE01 saw exactly this class of failure.

### Expected event stream

```
press → click:pressed (+ mousePress)
      → drag:drag-begin (sequence claimed)
      → drag:drag-update × n  (offsets from press point, any position)
      → drag:drag-end        (inside the ButtonRelease event)
      → mouseRelease applies the drag
```

### Coordinate spaces (measured)

- `drag-begin` args: widget-relative press point.
- `drag-update`/`drag-end` args: **offsets** from the press point (not
  absolute coords).
- `gtk_gesture_drag_get_start_point`: widget-relative press point —
  absolute widget pos = start + offset; valid off-band (negative /
  >allocation) and must stay signed.
- `gdk_event_get_position` (in the cancel trace): **surface/native**
  coords — hence (73,385) surface ↔ (72,222) left-ruler widget px.
- XP layer receives `tlu(ev_x), tlu(ev_y)` layout units.

## Secondary observations (not this bug)

- `fe drag-CANCEL ... evtype=-1` at the end of the double-click phase is
  a controller reset outside event dispatch (teardown) — benign.
- A separate, rarer flake — `probe found no tab near 392` /
  `press never grabbed the new tab` / `real press did not grab marker` —
  is a hit-test/layout-settling miss, not the gesture-cancel path
  (drag-end there arrives normally on a real release). Worth its own row
  if it persists after RUL03.

## Ranked suspects → root cause verdict

1. **Confirmed root cause**: synthetic `gdk_surface_ensure_motion`
   `MOTION_NOTIFY` (state=0, time=0) resets `GtkGestureSingle` mid-drag
   (`button == 0` → `gtk_event_controller_reset`), and `drag_end`
   loses the release because `gtk_gesture_drag_get_start_point()` fails
   on the cancel-emitted END. Mechanism verified end-to-end against GTK
   4.14 sources and the `evtype=1/evstate=0/evtime=0` trace.
2. ~~Gesture competition Click-vs-Drag claiming the sequence~~ — ruled
   out: claim produces `seqstate=DENIED`, not CANCEL; the observed
   trigger is a motion event, not a claim.
3. ~~Off-band release not delivered to the ruler~~ — ruled out:
   `gtk_pointer_focus_get_effective_target` routes grab-sequence events
   to the grab widget; passing runs show release reaching `drag-end`
   fine even far off-band.
4. ~~Coordinate-space bug~~ — ruled out: offsets convert correctly
   (ev_y=2664lu → APPLY page-margin-top on pass; delta 960 lu matches).

## Candidate fixes for RUL03 (pick ONE)

- **A. (preferred) Cache the drag start point.** Store
  `m_dragStartX/m_dragStartY` in `drag_begin`; compute absolute
  position as `cached start + offset` in `drag_update`/`drag_end`
  instead of calling `gtk_gesture_drag_get_start_point`. In `drag_end`,
  deliver `mouseRelease` whenever a `drag_begin` preceded (the emitted
  offsets remain correct on the cancel path — they come from stored
  priv->start_x/last_x, not the live point). Smallest change, survives
  the whole class of cancel-before-end races. Optionally guard with a
  "began and not already ended" flag to keep it idempotent.
- **B. Complete the release inside `cancel`.** In the `cancel` handler
  the sequence is still attached (emitted before the point is stolen),
  so `get_start_point`/`get_offset` may still succeed — finish the drag
  there and suppress the following dead `drag-end`. More fragile than A:
  relies on cancel-emission ordering.
- **C. Replace GtkGestureDrag with `GtkEventControllerLegacy`** and hand
  roll press/motion/release filtering on the sequence — most robust to
  GTK quirks but the largest rewrite and reintroduces the off-band grab
  problem GtkGestureDrag was chosen to solve.
- (Not viable: suppressing the synthetic motion — `request_motion` is
  GTK-internal scheduling we cannot disable; likewise gesture
  claiming/phases don't prevent single-gesture `button==0` reset.)

## Scope note

RUL02 implemented **no product fix** — only `RULER_TRACE`-gated
diagnostic instrumentation (drag `cancel` handler + richer
`drag-end`/`drag-cancel` fields in `ap_UnixRuler.cpp`,
`leftRuler` traces in `ap_LeftRuler.cpp`), all silent unless
`RULER_TRACE=1` is set. The fix itself belongs to RUL03.

## RUL03 resolution

Candidate A landed. `ap_UnixRuler` caches `m_dragStartX/Y` at
`drag-begin` and `m_dragLastX/Y` at every `drag-update`; `drag_end`
delivers `mouseRelease` whenever a begin preceded — `start + offset`
when the current event is a real `GDK_BUTTON_RELEASE`, else the last
tracked position (the cancel path can emit zeroed offsets). Verified:
`ui-drive --ruler` under Xvfb 10/10 runs clean — every run applies
`delta 960 lu` on both the top-ruler indent off-band drag and the
left-ruler margin off-band drag; the `drag-CANCEL -> drag-end ->
delta 0` signature is gone.

Two follow-on findings worth remembering:

- **GTK4 can emit `drag-begin` BEFORE the click controller's
  `pressed` for the same sequence** (the drag gesture claims the
  button-press immediately). Any per-press reset of drag bookkeeping
  must not clear state the same sequence's `drag-begin` already set —
  clearing `m_bDragBegun` on press reintroduced the exact release-loss
  signature the cache was added to fix. Clearing `m_bDragClaimed`
  there is still correct: a stale claim would suppress the next real
  click's release, and a same-sequence clear at worst produces a
  redundant `mouseRelease` that the XP layer no-ops on
  `m_bValidMouseClick`.
- **ui-drive probe churn moved the target.** The old tab-grab probe
  located a committed tab by pressing every px; each free-zone press
  creates and drops a pending tab, each drop calls `setBlockFormat`,
  and the resulting reformat cascade shifts the column origin — the
  probe moved the very box it was measuring (~+50px impulsive jumps
  observed mid-aim). Fixed by `AP_TopRuler::tabStopIndexAtXForTest`,
  a side-effect-free `_findTabStop` wrapper the driver's
  `scan_tab_box` uses, plus waiting for the scanned box position to
  hold still across ~120ms reads and re-aiming after the pointer warp
  (the warp's own pump can flush a queued resize/scroll that shifts
  the origin). The free-zone click target is now the center of a
  >=120px free run rather than the zone's left edge, where `xrel~0`
  used to snap the committed tab to `0pi` at the column origin.
