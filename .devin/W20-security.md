# W20 — Final gate 2/4: security audit

Date: 2026-10-18
Scope: control channel (`xap_UnixControlSocket.cpp`), template/style
loading (`attachTemplate`/`updateStyles`), TOC source-strux lifetime +
Ctrl+Click navigation, and the stream's other new input paths (find
patterns, table formulas, clipboard ring, content controls, `=rand`,
artwork insertion).

## Threat model

The control channel is a localhost (127.0.0.1-only) JSON-lines socket,
off by default (`--control=PORT` / `ABINOVA_CONTROL_PORT`). Its purpose
is test automation — any authenticated caller can run every edit method
and read/write documents, so authentication IS the security boundary.
The realistic attacker is a *different local user* (or any process able
to reach loopback, e.g. a browser hitting 127.0.0.1) who does not hold
the key. Document-borne hostile input (formula params, `=rand`, TOC
structure, template files) is a second boundary: files opened from
untrusted sources must not crash or escalate.

## (a) Control channel — findings fixed

| # | Finding | Severity | Fix |
|---|---------|----------|-----|
| A1 | Unbounded pre-auth receive buffer: `g_data_input_stream_read_line_async` assembles the whole line *before* the 1 MiB check — a client that never sends `\n` grows heap without limit | high (local DoS) | Rewrote the reader on bounded `g_input_stream_read_bytes_async` chunks (64 KiB) + explicit `std::string buf`; the 1 MiB cap is now enforced *while* buffering — an endless line can never grow the buffer past ~1 MiB, then the connection is closed after "line too long" |
| A2 | Connection teardown could free `_Conn` under an in-flight read callback (auth-timeout or close-after-reply while `read_line_async` pending → use-after-free) | high (UAF) | `_Conn` is now refcounted: one session ref + one per in-flight read; `_conn_close` sets `closing`, cancels `cancellable`, drops the session ref; the cancelled callback checks `closing` before touching state and drops its own ref |
| A3 | Auth key generated with `g_random_int_range` — a non-crypto PRNG; a second local user who can guess/seed the sequence could forge the key | high (auth bypass) | Key now built from `UT_go_random_bytes` (BCrypt/arc4random_buf/getrandom//dev/urandom); startup fails closed when no OS entropy is available |
| A4 | Key file written by `g_file_set_contents` then `g_chmod(0600)` — a permissive umask leaves a window where the key is world-readable; the write also follows symlinks | medium (key disclosure) | `g_open(O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW, 0600)` — private from creation, refuses symlink targets; short write/failure tears the service down instead of serving with a half-written key |
| A5 | Key compare was `std::string ==` — early-exit byte compare gives a loopback client a timing side channel | low | `_key_match` — constant-time-ish compare over the longer length (documented best-effort; see residual R3) |
| A6 | `attachTemplate` absent from the read-path confinement list — its stored `attached-template` metadata is later read by `updateStyles` with empty `params.data`, bypassing the root check | medium (confinement bypass) | Added to `_cmd_reads_path`; verified end-to-end (`/etc/passwd` rejected at store time) |
| A7 | Missing private dir fell through to writing `/control-key.local-N` at filesystem root | low | mkdir failure now fails `abi_control_start` closed |
| A8 | `_conn_send` used `g_output_stream_write` — a short write truncates the reply stream and desyncs every later response; `c->out` stayed dangling after close | low | `g_output_stream_write_all` + `out = nullptr` on close + send guard |

### File-touching surface — path confinement coverage

Write-checked: `file.save`, `ui.screenshot`, `ui.render`,
`engine.execute` → `fileSave*`/`fileExport`/`fileSaveImage`/
`fileSaveAsWeb`/`fileSaveTemplate`.
Read-checked: `engine.execute` → `openRecent`, `updateStyles`,
`attachTemplate`, `pageWatermark`'s embedded `image <path>`.
Checks resolve through `realpath`/canonicalization, handle `file://`
URIs, and enforce a root-prefix boundary (`root + '/'`), so `..`,
symlinks and root-sibling prefixes (`/root2`) all fail closed.
When no `--automation-{read,write}-root` is configured the channel is
deliberately full-trust (the automation contract); the confinement
fixes matter precisely when the operator opts into roots.

### Fuzz results (live instance, Xvfb :98)

20-case Python client against `--control=17371
--automation-read-root=/tmp/w20in --automation-write-root=/tmp/w20out`:

- non-JSON `GET /` line → `bad request` + close (HTTP probe can't reach
  the dispatcher)
- truncated `{"method":` line → rejected, no crash
- **2 MiB unterminated line → `line too long` + close, server alive**
  (pre-A1 this buffered the full stream indefinitely)
- EOF on a partial line → clean close
- wrong key on `auth` and on a data-bearing request → `unauthorized`
  + close
- real auth + ping → `pong`; 3 newline-separated requests in one TCP
  write → 3 replies in order
- traversal blocked with `outside automation-{read,write}-root` on:
  `file.save` → `/etc/…`, `ui.screenshot` → `/etc/…`,
  `attachTemplate` → `/etc/passwd`, `updateStyles` → `/etc/passwd`
- `insertShape` `../../etc/passwd` → refused by the artwork guard
- nested quotes / `\\"` escapes / NUL-prefixed garbage → fail-safe
- 60-connection rapid churn → server alive and still serving

## (b) attachTemplate / updateStyles

- Path handling: `attachTemplate` stores caller text verbatim in
  `attached-template` metadata; `updateStyles` reads that path (or its
  own `params.data`) through the normal `PD_Document::readFromFile`
  importer chain. Importer robustness against hostile file *content* is
  covered by the fuzz stream (W-series corpora); W20's fix confines the
  *path* when roots are configured (A6).
- Style payloads are plain `name;type;basedon;followedby;props`
  attribute vectors — no format strings, no code paths from style text;
  `setAllAttributes`/`appendStyle` treat them as data. No bounds issues
  found.

## (c) TOC source-strux lifetime

- `TOCEntry::m_pSourceSDH` is a non-owning `pf_Frag_Strux*` captured at
  TOC build time; deleting/merging the source heading unlinks the frag
  and frees its storage, so `getSourcePosForGenBlock` →
  `getStruxPosition` dereferenced a dangling pointer on Ctrl+Click
  navigation.
- **Fix:** new `pf_Fragments::contains(const pf_Frag *)` pointer-
  identity membership check over the live tree; the navigator now
  resolves only while the handle is still on the list.
- Verified end-to-end over the control channel: open `toc.abw`, delete
  the source heading (TOC regenerates/merges), then a real XTest
  ctrl+click on the entry — navigation resolves and the process
  survives.

## (d) Other new input paths

| Path | Finding | Status |
|------|---------|--------|
| Table formulas (`table_formula` field param, e.g. `=SUM(A1:B2)`) | recursive-descent parser recursed unboundedly on `(` groups, unary chains, `^` and calls — a hostile doc could stack-overflow on load/repaint | **FIXED:** `DepthGuard`, max depth 64 → `FMLERR_SYNTAX`. Verified: doc with 20000-deep parens / 20000 unary / 5000 `^` exports to PDF cleanly |
| `insertShape`/`insertIcon`/`insert3DModel` | `params.data` became a subdir path under `artwork/` — `..` escaped the installed dir (e.g. `../../etc/...`) | **FIXED:** `..` rejected in `s_insertArtworkFile`; absolute inputs collapse to a subdir component, extensions are caller-appended; verified refused on the live channel |
| `=rand(p,s)` / `=lorem(p,s)` | command line capped at 64 chars; counts clamped to Word's own bounds (200×99); digit parsing saturates at 999 | already bounded — no change |
| Clipboard history ring | 20 entries × 256 KiB per-item cap, whitespace-only rejected, move-to-front dedup | already bounded — no change |
| Content controls (`sdt-*`) | `key=value` pair parser → plain string attributes on the object strux; no path/format/alloc hazards | clean — no change |
| Find patterns (`ut_findpattern`) | bounded char classes, escaped literals, group-ref guards; no backtracking engine | clean — no change (audit only) |
| `ui.resize`/`ui.click`/`ui.key` | numeric fields clamped (16..16384, 0..16384, button 1..9); X11-only path guarded | clean — no change |
| Secrets in logs | key never logged; only the port is `g_message`'d; DEBUGMSG sites pass user strings as args, never as format | clean |
| Integer overflow | `strtol` field extraction + clamped consumers; `readNum` saturates at 999; no unbounded multiplications found | clean |

## Bonus fixes (pre-existing portguard failures found during verify)

- `src/af/xap/xp/xap_Frame.cpp`: `#include <unistd.h>` flagged by the
  portability leak guard — the two `close()` calls it needed are now
  `g_close()`; include dropped.
- `src/af/xap/gtk/xap_UnixControlSocket.h`: `/tmp/shot.png` literal in a
  doc comment → `<output-dir>/shot.png`.
- `tools/portguard.sh` now reports OK (0 leaks, baseline clean).

## Residual risks (documented, not fixed)

- **R1 — naive JSON field extraction.** `_json_str`/`_json_num`/
  `_json_bool` do substring matching, so a `"key"` inside a string value
  can be picked up as a field. Not exploitable upward: pre-auth only the
  `auth` method runs, and a field embedded in a data string can only
  *supply* a key the attacker already knows — it cannot extract the real
  one. Escape handling is fail-safe (`\uXXXX` degrades to literals;
  `\/`→`/` is a plain separator). The checked value and the used value
  come from the same extraction, so confinement can't be disagreed past.
  A real JSON parser is nice-to-have, not a gate.
- **R2 — `contains()` is pointer-identity.** If a freed frag address is
  immediately reused for a different live frag (ABA), a stale handle
  resolves to a wrong-but-live strux → wrong navigation target, not a
  memory-safety issue. True weak handles would need piece-table
  ownership changes — out of scope.
- **R3 — `_key_match` is best-effort.** The compiler could still
  shorten the loop; loopback timing jitter already dwarfs any per-byte
  delta, and the fallback remains "auth fails → close". Accepted.
- **R4 — synchronous replies.** `_conn_send` blocks on a non-reading
  client; replies are tiny and the endpoint is loopback-authenticated
  automation. Accepted.
- **R5 — stale key files.** Dead sessions leave
  `control-key.local-<port>` files in the 0700 private dir (0600 each).
  Readable only by the same uid; a later session on the same port
  overwrites. Startup sweep skipped deliberately — a second live
  instance's key must not be deleted. Accepted.
- **R6 — document-borne `attached-template`.** A hostile document can
  carry the metadata prop that `updateStyles` reads when invoked with
  empty data; the file is then run through the importer (same exposure
  as opening it). Path confinement applies to *channel-supplied* paths
  only. Accepted.

## Verification

- `cd src && make -j2` — clean.
- Live control-channel fuzz: all fail-safe, server survives (see above).
- Hostile formula doc → PDF export clean, no crash.
- TOC ctrl+click navigation exercised via XTest — resolves, no crash.
- `make check`: testwrap 18440 asserts / **5 fails = pre-existing
  timing-flaky `fv_DragAutoScroll`** (documented in WORKLOG W15);
  rtwrap PASS; **portwrap PASS** (was failing on the two pre-existing
  leaks fixed above); dlgswrap PASS; drvwrap running at report time.
