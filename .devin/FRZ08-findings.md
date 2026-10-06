# FRZ08 — Wayland freeze: audit + live reproduction

Date: 2026-10-06. Session: live GNOME/Zorin Wayland (`wayland-0`,
`XDG_RUNTIME_DIR=/run/user/1000`), GTK 4.14.5.

## Reproduction (part b — confirmed)

`ui-drive --id 1004` (Document dialog, drives GtkDropDowns/popovers)
on the live `wayland-0` session with `GDK_BACKEND=wayland`:

- Runs 1–3 clean (rc=0).
- Run 4 captured the wedge the user reports. Phase tracker printed:

      drive: signal 14 in phase: interact GtkPopover name=GtkPopover
      poll
      wl_display_dispatch_queue          libwayland-client
      libgtk-4 +0x4334f7                 popup-layout wait loop
      libgtk-4 +0x1d5da2  +0x123bd6
      gtk_widget_allocate                (GtkPopover size-allocate)

  Disassembly of libgtk +0x4334f2 shows a `wl_display_dispatch_queue`
  loop wrapped around `gdk_popup_layout` ref/unref handling — GTK's
  Wayland popup "latch layout" wait for the compositor's
  `xdg_popup.configure`. When the configure never arrives (popup
  raced with a popdown, parent surface unmapped, or a denied grab)
  the dispatch loop polls the wayland fd forever and the whole UI
  thread freezes — exactly the FRZ05 signature.

- The wedge survived >40 s (the drive's interact alarm rescued it;
  a real user has no alarm — permanent "not responding").

## Audit findings (part a)

Blocking-compositor-wait surfaces in OUR code:

| Site | Verdict |
|------|---------|
| `XAP_UnixFrameImpl::_runModalContextMenu` (xap_UnixFrameImpl.cpp) | **BUG — fixed.** Unbounded `g_main_loop_run` waiting only for GtkPopover `"closed"`, which is not guaranteed (denied popup, teardown, destroy mid-wait never emit it). Also popped up without checking the toplevel was mapped — an `xdg_popup` on an unmapped parent can never be configured → the captured `wl_display_dispatch_queue` wedge. Now: skips the popup when the toplevel isn't mapped; loop quits on closed/unmap/destroy; 15 s present-watchdog bails if the popover never maps; signal/weak-ref hygiene. |
| `abiRunModalDialog` (xap_UnixDialogHelper.cpp) | **BUG — fixed.** Nested loop quit only on `response`/`close-request`; a dialog destroyed without a response (external teardown, session end) left it looping forever. Now connects `destroy` → quit. Also disconnects the `&run`-capturing handlers before return — they were latent dangling-pointer callbacks if the dialog outlived the call. |
| `XAP_UnixDialog_PrintPreview::runModal` | **BUG — fixed.** Same destroy-gap; additionally `gtk_window_destroy(m_pWindow)` after the loop could touch a finalized widget. Now weak-pointers the window, quits on destroy, destroys only if still ours. |
| `XAP_UnixClipboard` read loops | OK — timeout + abandon path. |
| Drop-read loop (`s_drop_cb`) | OK — `ABI_DROP_TIMEOUT_MS`. |
| Screenshot portal loop | OK — owner-watch + 600 s failsafe. |
| `_nullUpdate` / `xap_UnixDlg_ClipArt` pumps | Bounded (5 iterations / dir count). Pumps *deliver* the pending configure, so they are not the wedge; reentrancy covered elsewhere. |
| `xap_gtk_popover_new` | Already disables autohide grabs (`autohide=FALSE`) — avoids the xdg_popup grab waits. |
| `XFlush` (ap_UnixFrameImpl arrangeAll) | X11-gated (`GDK_IS_X11_DISPLAY`). |
| `gdk_display_sync/flush`, `wl_display_*` direct calls | None in tree. |
| `gtk_widget_realize` at frame construction | Compositor always answers EGL/dmabuf roundtrips — not a wedge. |

## Residual / GTK-side

The wedge itself is inside libgtk (GtkDropDown/GtkMenuButton internal
popovers can't be intercepted — our `gtk_popover_popup` call sites
are now parent-mapped-guarded where it matters: the context menu).
If the compositor drops an `xdg_popup.configure`, GTK blocks forever
in `gtk_widget_allocate`; upstream GTK tracks this class (popup
latch-layout deadlock). Our fixes remove every OUR-side path that
can wait forever on compositor-driven signals.

## Verification

- `make -j2` clean.
- Live wayland-0: `ui-drive --id 1004` ×3 rc=0 post-fix.
