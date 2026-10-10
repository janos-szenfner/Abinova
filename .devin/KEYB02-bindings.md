# KEYB02 — Full keybinding audit vs MS Word

Scope: every shortcut surface that ships in the default map
(`src/wp/ap/xp/ap_LB_Default.cpp` — `NVKTable`, `NVKTable_P` dead-key
prefixes, `CharTable`), how chords resolve through
`EV_EditEventMapper`/`EV_EditBindingMap`, and whether they actually
fire through the real GTK/X11 input path
(`GtkEventControllerKey` → IM prefilter → `ev_UnixKeyboard::keyPressEvent`
→ `EV_EditEventMapper::Keystroke` → `EV_EditMethod`).

Menu accelerators (`ev_UnixMenu`/`ap_Menu_ActionSet`) are display-only:
they *show* the shortcut label resolved from the binding map, they do
not execute it — the EV keyboard layer is the single execution route.
The ribbon has no GtkMenuBar accel layer, so nothing relies on menu
accel registration.

macOS: `ev_UnixKeyboard::keyPressEvent` folds
`GDK_META_MASK`/`GDK_SUPER_MASK` (⌘ under Quartz/XQuartz) into
`EV_EMS_CONTROL`, so every Ctrl row doubles as its Cmd row. The three
documented platform splits (`__APPLE__`) are the only places where Cmd
must differ from Word's Ctrl semantics (Cmd+Q quit, Cmd+Shift+Z redo,
Option+Delete word-left). The Windows build shares this same table;
Windows key names map onto the same NVK/char encoding.

## Architecture notes

- `CharTable` lookup strips Shift from the modifier-column index
  (`EV_EMS_ToNumberNoShift`) — Ctrl+Shift+`x` resolves through the
  *uppercase* `0x58 'X'` row's `_C` column. Uppercase rows are live
  code, not dead rows.
- `NVKTable` columns: `{ none, _S, _C, _S_C, _A, _A_S, _A_C, _A_C_S }`.
- The GTK IM prefilter (`xap_UnixFrameImpl::_fe::key_press_event`)
  passes Alt/Super/Hyper/Meta chords to the app even when an IM claims
  the event; Ctrl chords rely on the IM declining (Wayland defers to
  the compositor; X11/IBus reinjects unclaimed keys exactly once —
  audited under KEYB01, no swallow observed).

## Bindings changed toward Word parity

| Chord | Was | Now | Word |
|---|---|---|---|
| Ctrl+Shift+A | selectAll | **toggleAllCaps** (new method) | All Caps |
| Ctrl+Shift+C | copy | **formatPainter** | copy formatting |
| Ctrl+Shift+F | find | **dlgFont** | Font dialog |
| Ctrl+Shift+G | go | **dlgWordCount** | Word Count |
| Ctrl+Shift+H | replace | **toggleHidden** | hidden text |
| Ctrl+Shift+K | insertHyperlink | **toggleSmallCaps** (new method) | small caps |
| Ctrl+Shift+L | alignLeft | **doBullets** | bullets |
| Ctrl+Shift+P | print | **dlgFont** | Font dialog |
| Ctrl+Shift+T | toggleOline | **unHangingIndent** (new method) | unhang indent |
| Ctrl+T | toggleOline | **hangingIndent** (new method) | hanging indent |
| Ctrl+Shift+Z | undo | **clearFormatting** (non-macOS) | ResetChar |
| Ctrl+[ | editHeader | **fontSizeDecrease** | shrink font 1pt |
| Ctrl+] | editFooter | **fontSizeIncrease** | grow font 1pt |
| Alt+Backspace | delBOW | **undo** (non-macOS) | undo |
| F4 | — | **redo** | repeat/redo |
| Ctrl+Shift+F5 | — | **insertBookmark** | bookmark |
| F9 | unbound row | **updateField** | update field |
| Ctrl+F9 | unbound row | **insField** | insert field |

Platform splits kept/added (`__APPLE__`):

- Cmd+Q → `querySaveAndExit`; Ctrl+Shift+Q stays `clearParaFormatting`
  on every platform. On Linux/Windows Ctrl+Q itself is
  `clearParaFormatting`.
- Cmd+Shift+Z → `redo`; Ctrl+Shift+Z → `clearFormatting` elsewhere
  (Word ResetChar; Ctrl+Y and F4 are the redo chords).
- Option+Backspace → `delBOW` (word-left); Alt+Backspace → `undo`
  elsewhere.

New edit methods in `ap_EditMethods.cpp`: `hangingIndent` /
`unHangingIndent` (±0.5in `margin-left`/`text-indent`, matching the
ruler step), `toggleAllCaps` (`text-transform:uppercase`),
`toggleSmallCaps` (`font-variant:small-caps`).

## Product bug found and fixed by the audit

**Ctrl+Q never cleared paragraph formatting** — `FV_View::resetBlockFormat()`
issues `changeStruxFmt(PTC_AddFmt, …, {"props",""}, …)`, but
`PP_AttrProp::areAlreadyPresent` explicitly skipped the `props` name in
its empty-value removal check, so the merge short-circuited as a no-op
whenever the AP held properties. `resetCharFormat` only escaped because
its `style` companion attribute trips the same check. Fixed in
`pp_AttrProp.cpp`: an empty-valued attribute (including the literal
`props` name, which `setAttribute` expands rather than stores) now
reports "not already present" while properties remain, so the
`bIgnoreProps` clear path in `cloneWithReplacements` actually runs.

## Complete audited map

### Editing / file

| Chord | Bound to | Word | Status |
|---|---|---|---|
| Ctrl+Z / Ctrl+Y / F4 | undo / redo / redo | same | ok |
| Alt+Backspace | undo (delBOW on macOS) | undo | fixed |
| Ctrl+X/C/V | cut / copy / paste | same | ok |
| Shift+Delete / Shift+Insert / Ctrl+Insert | cut / paste / copy | same | ok |
| Ctrl+Alt+V | pasteSpecial | same | ok |
| Ctrl+Shift+C / Ctrl+Shift+V | formatPainter | copy/paste formatting | ok |
| Ctrl+A | selectAll | same | ok (KEYB01) |
| Ctrl+N/O/S/W | fileNew / fileOpen / fileSave / closeWindow | same | ok |
| Ctrl+Shift+S / F12 | fileSaveAs | Save As | ok |
| Shift+F12 | fileSave | Save | ok |
| Ctrl+F12 / Ctrl+O | fileOpen | Open | ok |
| Ctrl+P / Ctrl+Shift+F12 | print | Print | ok (env-limited, see below) |
| Ctrl+F2 | printPreview | same | ok |
| Alt+F4 | querySaveAndExit | quit | ok |
| Ctrl+F4 | closeWindow | close doc | ok |

### Find / fields / review

| Chord | Bound to | Word | Status |
|---|---|---|---|
| Ctrl+F / Ctrl+H / Ctrl+G / F5 | find / replace / go | same | ok |
| F3 | findAgain | repeat find | ok |
| F7 | dlgSpell | spelling | ok |
| Ctrl+; | dlgSpell | Cmd+; spell | ok |
| F9 / Ctrl+F9 | updateField / insField | same | **added** |
| Ctrl+Shift+F5 | insertBookmark | bookmark | **added** |
| Ctrl+Shift+E | toggleMarkRevisions | track changes | ok |
| Alt+↑ / Alt+↓ | prevComment / nextComment | prev/next comment | ok |

### Character formatting

| Chord | Bound to | Word | Status |
|---|---|---|---|
| Ctrl+B/I/U | toggleBold/Italic/Uline | same | ok |
| Ctrl+D / Ctrl+Shift+F / Ctrl+Shift+P | dlgFont | Font dialog | ok |
| Ctrl+Shift+A | toggleAllCaps | all caps | **added** |
| Ctrl+Shift+K | toggleSmallCaps | small caps | **added** |
| Ctrl+Shift+H | toggleHidden | hidden | ok |
| Ctrl+Shift+X | toggleStrike | strikethrough | ok |
| Ctrl+= / Ctrl+Shift+= | toggleSub / toggleSuper | sub/superscript | ok |
| Ctrl+Space | togglePlain | clear char fmt | ok (KEYB01) |
| Ctrl+Shift+Z | clearFormatting | ResetChar | **fixed** |
| Ctrl+Shift+> / < | fontSizeIncrease/Decrease | grow/shrink | ok |
| Ctrl+[ / Ctrl+] | fontSizeDecrease/Increase | ±1pt | **fixed** |

### Paragraph

| Chord | Bound to | Word | Status |
|---|---|---|---|
| Ctrl+E/L/R/J | alignCenter/Left/Right/Justify | same | ok |
| Ctrl+M / Ctrl+Shift+M | toggleIndent / toggleUnIndent | indent/unindent | ok |
| Ctrl+T / Ctrl+Shift+T | hangingIndent / unHangingIndent | hanging indent | **added** |
| Ctrl+Q | clearParaFormatting | clear para fmt | **fixed** (product bug) |
| Ctrl+0 | toggleParaBefore | space before | ok |
| Ctrl+1/2/5 | singleSpace/doubleSpace/middleSpace | line spacing | ok |
| Ctrl+Shift+N | setStyleNormal | Normal style | ok |
| Ctrl+Shift+L | doBullets | bullets | **fixed** |
| Alt+Ctrl+1/2/3 | setStyleHeading1/2/3 | Heading 1-3 | ok |

### Navigation / selection

| Chord | Bound to | Word | Status |
|---|---|---|---|
| Home/End, Ctrl+Home/End | warp BOL/EOL, BOD/EOD | same | ok |
| Ctrl+←/→ | warpInsPtBOW/EOW | word left/right | ok |
| Ctrl+↑/↓ | warpInsPtBOB/EOB | para up/down | ok |
| PgUp/PgDn, Ctrl+PgUp/PgDn | screen/page warps | same | ok |
| Shift+arrows/Home/End/PgUp/PgDn | extSel* | extend selection | ok |
| Ctrl+Shift+arrows/Home/End | extSelBOW/EOW/BOB/EOB/BOD/EOD | same | ok |
| Option+←/→ (macOS) | warpInsPtBOW/EOW | word left/right | ok |
| Shift+F10 / Menu key | contextMenu | context menu | ok |
| Alt+F10 | selPane | select pane | ok |

### Insertions

| Chord | Bound to | Word | Status |
|---|---|---|---|
| Enter / Shift+Enter | insertParagraphBreak / insertLineBreak | same | ok |
| Ctrl+Enter / Ctrl+Shift+Enter / Alt+Enter | page / column / section break | same | ok |
| Tab / Shift+Tab | insertTab / insertTabShift | same | ok |
| Ctrl+Tab | insertTabCTL | literal tab | ok |
| Ctrl+Shift+Space | insertNBSpace | nbsp | ok |
| Ctrl+- / Ctrl+Shift+- | insertSoftHyphen / insertNBHyphen | opt./nb hyphen | ok |
| Ctrl+Alt+- / Ctrl+Alt+Shift+- | insertEmDash / insertEnDash | em/en dash | ok |
| Ctrl+Alt+C/R/T | © / ® / ™ | same | ok |
| Insert | toggleInsertMode | overtype | bound — inert while `InsertModeToggle` pref = 0 (documented) |

### Windows / misc

| Chord | Bound to | Word | Status |
|---|---|---|---|
| Ctrl+F6 / Ctrl+Shift+F6 / Ctrl+Tab | cycleWindows / cycleWindowsBck | cycle docs | ok |
| F11 | viewFullScreen | focus mode | ok |
| F1 | helpContents | help | ok |
| Alt+F8 | executeScript | macro | ok |
| Ctrl+Alt+P/N/S | print layout / normal layout / split | same | ok |
| Ctrl+, | dlgOptions | options | ok |
| Ctrl+Shift+G | dlgWordCount | word count | **fixed** |
| Ctrl+Alt+F / Ctrl+Alt+D / Ctrl+Alt+M | footnote / endnote / comment | same | ok |

## Intentional divergences (documented, not bugs)

- **Ctrl+Shift+S** → `fileSaveAs` (LibreOffice-style Save As; Word uses
  it for the Apply Styles pane — Save As is the more useful map and
  F12 covers Word's own Save As chord anyway).
- **Ctrl+Shift+D / Ctrl+Shift+P** → `dlgFont` — Word maps these to
  double-underline / font-size dropdown which have no single-method
  equivalents; the Font dialog covers both.
- **Insert** → `toggleInsertMode` resolves but is inert while the
  `InsertModeToggle` preference is `0` (default); with the pref enabled
  it toggles both directions.
- **Ctrl+P / Ctrl+Shift+F12 → print** — the binding and method are
  verified through `EV_EditEventMapper` resolution; real injection is
  not exercised in CI because `GtkPrintUnixDialog` blocks inside
  libgio's session D-Bus/CUPS queries under xvfb (same root cause as
  the drvwrap dialog-id-8 xfail — no Abinova code at fault).
- **Ctrl+O** — resolves to `fileOpen` (same dialog as Ctrl+F12, which
  IS injected); not double-injected in the leg to avoid a second
  native-chooser round trip.
- **Emacs/vi maps** unchanged — they are opt-in alternates, not the
  Word-parity surface.

## Verification

- `Abinova-test ap_KeyBindings` — every audited chord resolved through
  the live `EV_EditBindingMap`: **177 asserts, 0 failures**
  (extended in this task to cover all new bindings + the two platform
  splits).
- `ui-drive --keys` under `xvfb-run` — real XTest injection through
  `GtkEventControllerKey` → IM prefilter → `ev_UnixKeyboard`: Ctrl+A
  whole-doc selection incl. from a header/footer session, typing/navigation
  deltas, char + para formatting assertions (bold/italic/underline,
  case toggles, indents incl. Ctrl+T/Ctrl+Shift+T hanging indents,
  Ctrl+M/Ctrl+Shift+M, Ctrl+Q clear-para — now passing after the
  `areAlreadyPresent` fix), clipboard, undo/redo, 19 dialog chords
  observed opening real toplevels and being answered by the stray
  sweep, Ctrl+W dirty-close prompt. **PASS**; wired into
  `drvwrap.sh` as the `keys` leg on the minimal
  `test/wp/cov07/keys_min.abw` fixture.
- `Abinova-test` ptbl/fmt suites for the `areAlreadyPresent` change:
  pt_DocEdits + pt_PieceTable + pf_Fragments + pp_PropertyMap +
  pd_Revision **810 tests, 0 failures**; fv_EditOps + fv_ViewOps +
  fv_TableOps + fv_PasteTag **365 tests, 0 failures**.
