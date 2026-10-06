# FRZ06 — aggregate drvwrap dialog sweep results

Date: 2026-10-06
Command: `./src/wp/test/unix/drvwrap.sh > /tmp/frz06-sweep.log` — one
background command; the script self-re-execs under `xvfb-run -a` after
unsetting `WAYLAND_DISPLAY`/`GDK_BACKEND` (TST11 hygiene). Build tree
already current (`make -j2` = no-op). Sweep duration ~8 min.

## Summary

| Leg | Result |
|-----|--------|
| dialog sweep (71 ids, 90s timeout each) | **70/71 ok**, 0 skipped, 0 xfail, 1 fail (id 1005, rc=1) |
| `--frame` | timeout/fail (300s bound) — **expected**: owned by UI04/UI05 |
| `--abi` (abiwidget) | ok |
| `--ev` (event/menu/graphics) | ok |
| `--fmt` (text/fmt/gtk, wayland-preferred w/ x11 fallback) | ok |

drvwrap.sh exit status: 1 (one dialog fail). Every leg that passes
individually also passed inside the aggregate run — no mass timeouts,
no crashes, no 90s spins anywhere in the sweep.

## Per-id table

All ids completed under their 90s bound. Result = `ok` unless noted.

| id | result | | id | result | | id | result |
|----|--------|-|----|--------|-|----|--------|
| 1 | ok | | 10 | ok | | 19 | ok |
| 2 | ok | | 11 | ok | | 20 | ok |
| 3 | ok | | 12 | ok | | 21 | ok |
| 4 | ok | | 13 | ok | | 22 | ok |
| 5 | ok | | 14 | ok | | 23 | ok |
| 6 | ok | | 15 | ok | | 24 | ok |
| 7 | ok | | 16 | ok † | | 25 | ok |
| 8 | ok † | | 17 | ok | | 26 | ok |
| 9 | ok | | 18 | ok | | 27 | ok |
| 28 | ok | | 1012 | ok | | 1027 | ok |
| 29 | ok † | | 1013 | ok | | 1028 | ok |
| 30 | ok | | 1014 | ok † | | 1029 | ok |
| 1002 | ok | | 1015 | ok | | 1030 | ok |
| 1003 | ok | | 1016 | ok | | 1031 | ok |
| 1004 | ok | | 1017 | ok | | 1032 | ok |
| **1005** | **FAIL rc=1** ‡ | | 1018 | ok | | 1033 | ok |
| 1006 | ok | | 1019 | ok | | 1034 | ok |
| 1007 | ok † | | 1021 | ok | | 1035 | ok |
| 1008 | ok | | 1022 | ok | | 1036 | ok |
| 1009 | ok | | 1023 | ok | | 1037 | ok † |
| 1010 | ok † | | 1024 | ok | | 1038 | ok |
| 1011 | ok † | | 1025 | ok | | 1039 | ok † |
| 1040 | ok | | 1041 | ok † | | 1042 | ok |

† Previously-frozen/crashed mapped ids from FRZ05 — all now pass in
the aggregate sweep: 8 (Print — was the lone XFAIL), 16 (Insert
Symbol — was SIGSEGV rc=139), 29 (Merge Documents — was spinning),
1007 (Go To), 1010 (Paragraph), 1011 (Options), 1014 (Word Count),
1037 (LaTeX), 1039 (Borders & Shading), 1041 (RDF Editor).

‡ id 1005 = AP_DIALOG_ID_REPLACE (Find & Replace). rc=1 in a dialog
leg means `requestDialog()` returned NULL — the dialog never
constructed in that leg. Rechecked in isolation under forced x11
(`env -u WAYLAND_DISPLAY GDK_BACKEND=x11 xvfb-run -a ui-drive --id
1005`): **PASS** — "0 criticals, 1 warnings, 0 crashed, 0 wedged"
(the warning is a benign snapshot-without-allocation). Also passed
71/71 in TST11's and FRZ04's sweeps. Verdict: transient, not a
deterministic regression. Suspicion for FRZ07/UI04 context: the same
stray-Wayland contamination class — a manual rerun that leaked the
real-session wayland socket died on `xdg_wm_base error 4: wl_surface
already has a buffer committed`, i.e. GTK4 can reach the live
compositor even with `WAYLAND_DISPLAY` unset (default `wayland-0`
socket under `XDG_RUNTIME_DIR`).

## Notes for FRZ07

- No crashes (rc=139) and no timeouts (rc=124/137) anywhere.
- One transient fail (1005) — re-sweep or harden env scrubbing
  (`XDG_RUNTIME_DIR` isolation / `GDK_BACKEND=x11` pin) rather than
  dialog debugging.
- `--frame` leg remains the UI04 diagnosis item — do not treat its
  300s bound expiry as a regression here.
