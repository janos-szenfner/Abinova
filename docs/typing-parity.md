# Typing parity matrix

Word-parity typing behaviours, each pinned by a live control-channel
test in `tools/typing-parity.py`.  The driver launches a fresh
`src/abinova` per case under Xvfb (needs `xvfb-run`, `Xvfb` with
XTest, and `dbus-run-session`), types through the real XTest key path
(`ui.key`) or the `insertData` path (`ui.text`), and asserts on
`document.text` / `document.inspect` / `document.formatinspect`.

Run it:

```sh
python3 tools/typing-parity.py            # whole matrix
python3 tools/typing-parity.py --only list
```

`document.text` serialises block breaks as `\n` and list labels as
`\x07\t` (a field marker plus the label separator tab).

| # | Behaviour | Stimulus | Expected (Word parity) | Pinned result | Test case |
|---|-----------|----------|------------------------|----------------|-----------|
| 1 | Character insert | type `hello` | text lands at caret | `hello`; words 1, chars 5, paras 1 | `type-inserts-at-caret` |
| 2 | Real key path | keys `a` `b` `c` via XTest | identical to bulk insert | `abc` | `key-letters-insert` |
| 3 | Enter at EOL | `ab` ⏎ `cd` | splits paragraph | `ab\ncd`, paras 2 | `enter-splits-at-eol` |
| 4 | Enter mid-word | `hello`, ←←, ⏎ | splits at caret | `hel\nlo` | `enter-mid-word` |
| 5 | Enter at BOL | `abc`, Home, ⏎ | empty paragraph before | `\nabc` | `enter-at-bol` |
| 6 | BackSpace mid-text | `abc` ⌫ | deletes char left of caret | `ab` | `backspace-deletes-char` |
| 7 | BackSpace at doc start | ⌫ on empty doc | no-op | text and caret unchanged | `backspace-docstart-noop` |
| 8 | BackSpace at BOL | `ab` ⏎ `cd`, Home, ⌫ | joins paragraphs | `abcd`, paras 1 | `backspace-joins-paragraphs` |
| 9 | Delete (forward) | `abc`, Home, ⌦ | removes char right of caret | `bc` | `delete-forward-char` |
| 10 | Tab in body text | `a` ⇥ `b` | tab character inserted | `a\tb` | `tab-inserts-tabchar` |
| 11 | Select then type | `hello`, Ctrl+A, `bye` | selection replaced | `bye` | `select-all-type-replaces` |
| 12 | Select-type undo | … Ctrl+Z / Ctrl+Y | delete+insert is one atomic step | `hello` after one undo; `bye` after redo | `select-type-undo-atomic` |
| 13 | Typed-run undo granularity | keys `h`–`o`, Ctrl+Z | run coalesces into one record | undo 1 → `h`, undo 2 → `` (first insert into a fresh doc keeps its own record — `_canCoalesceInsertSpan` boundary) | `typed-run-coalesces-undo` |
| 14 | Bulk-insert undo steps | `insertData("hel")` + `insertData("lo")`, Ctrl+Z | each call is a user-atomic glob → its own undo step | `hel` then `` | `bulk-insert-undo-steps` |
| 15 | Smart quotes | type `"hi"` (pref `SmartQuotesEnable`, default on) | curly quotes | `say “hi” ok` | `smart-quotes` |
| 16 | List: Enter continues | `doNumbers`, `one` ⏎ `two` | next item auto-numbered | `\x07\tone\n\x07\ttwo` | `list-enter-continues` |
| 17 | List: Enter on empty item | … ⏎ on empty item | exits the list | `\x07\tone\n` + plain paragraph | `list-enter-empty-stops` |
| 18 | List: Tab demotes | ⇥ at a list item | demotes one level | context `level 1` → `level 2`, text unchanged | `list-tab-demotes` |
| 19 | Autoformat table | `+---+---+` ⏎ | becomes a 2-column table | `\n\n` + caret context "table cell" | `table-autoformat` |
| 20 | `=rand()` expansion | `=rand(1,2)` ⏎ | expands to sample paragraphs | pangram sentences, command consumed | `rand-expands` |
| 21 | Tab on paragraph selection | `hello world`, Ctrl+A, ⇥ | indents the paragraph 0.5" — no tab char | text unchanged, `margin-left` 0.5in | `tab-indents-paragraph-selection` |
| 22 | Tab on multi-paragraph selection | `one` ⏎ `two`, Ctrl+A, ⇥ | indents every covered paragraph | text unchanged, both paras indented | `tab-indents-multi-paragraph` |
| 23 | Tab on partial selection | `hello`, select `hell`, ⇥ | still replaces selection with a tab | `\to` | `tab-partial-selection-tabs` |
| 24 | Alt+numpad code | Alt+065 / Alt+8364 | inserts the decimal-code character | `A` / `€`; codes > U+10FFFF type nothing | `alt-numpad-character` |

## Notes on Alt+numpad and the input-method reset

* `ui.key` accepts comma-separated sequences (`alt+KP_0,KP_6,KP_5`) —
  modifiers in the first segment stay held for the whole sequence,
  which is what Alt+numpad entry needs.
* Alt+numpad lives in `xap_UnixFrameImpl` (`_altDigitKeyEvent`, shared
  by the canvas and a capture-phase toplevel controller): keypad
  digits accumulate while Alt is held and the character is inserted
  on Alt release, or on an ~800 ms pause after the last digit (the
  ribbon keytip controller can consume the bare Alt press/release —
  see `_s_kt_key`, which now passes keypad keys through). Both the
  NumLock-on (`KP_0..9`) and NumLock-off (`KP_Insert`…`KP_Prior`)
  keyvals count as digits.
* The IM context is reset only on context changes — canvas clicks and
  focus transitions (`resetIMContext`, `focusIMIn`). Ordinary
  IM-consumed keystrokes (letters, Space, Enter) no longer arm a
  reset the way the removed `need_im_reset`/`queueIMReset` flag did,
  so multi-keystroke compose sequences aren't killed by typing. No
  dead-key or compose-capable IM exists in the Xvfb test environment,
  so this is verified by code inspection plus the typing regression
  suite rather than a driven compose.

## Documented gaps vs Word

Pinned as *absent* so the gaps are deliberate, tested facts rather
than untracked drift:

| Behaviour | Word | Abinova (pinned) | Test case |
|-----------|------|-------------------|-----------|
| Shift+Tab on list item | promotes one level | no binding outside tables — nothing happens | `list-tab-demotes` |
| Auto-capitalise sentence starts | `hello.` → `Hello.` | not implemented — text stays as typed | `no-auto-capitalise` |
| Autocorrect as-you-type | `teh` → `the` | not implemented — text stays as typed | `no-autocorrect` |

## Notes on undo granularity

Undo records in `pt_PieceTable` coalesce only when a new span-change
record is directly contiguous in position, shares the attribute set,
and the history is already dirty (`_canCoalesceInsertSpan` /
`_canCoalesceDeleteSpan`).  Consequences the tests pin:

* a contiguous keystroke run is one undo step, except that the first
  insert into a fresh document keeps its own record;
* a bulk `insertData` is wrapped in a user-atomic glob and is always
  exactly one undo step, never merged with neighbours;
* select-then-type (delete + insert) is a single user-atomic glob, so
  one undo restores the original selection text.
