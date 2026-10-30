#!/usr/bin/env python3
"""typing-parity.py — W13 typing-parity matrix driver.

Pins Word-parity typing behaviours against a live Abinova instance
driven over the JSON-lines control channel (README "Automation —
runtime control channel").  Every case gets a fresh app launch so
undo history, prefs and document state can never leak between cases.

Input goes through two paths, asserted identical where it matters:

  * ``ui.text`` — the ``insertData`` edit method (bulk insert);
  * ``ui.key``  — real XTest key events on X11, the same path a
    physical keyboard takes (letters, Return, BackSpace, Delete,
    Tab, Ctrl+chords, Shift+Tab).

Assertions read back ``document.text`` (block breaks appear as
``\\n``, list labels as ``\\x07\\t``) and ``document.inspect`` /
``document.formatinspect``.

Needs an X server with XTest: run under ``xvfb-run`` or let the
script re-exec itself under ``xvfb-run -a dbus-run-session --``
(automatic when xvfb-run is on PATH and TP_CHILD is unset).

Usage:
    tools/typing-parity.py [--bin src/abinova] [--port N]
                           [--only REGEX] [--keep] [-v]

Exit code: 0 = all cases PASS, 1 = a case failed,
2 = infrastructure failure (no xvfb/display, app never came up).
"""

import argparse
import json
import os
import re
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TIMEOUT = 15.0          # reply deadline per request
SETTLE = 0.25           # post-action quiet time
LIST_MARK = "\x07\t"    # list label as it lands in document.text


def eprint(*a):
    print(*a, file=sys.stderr, flush=True)


class Channel:
    """One authenticated control-channel connection."""

    def __init__(self, port, key):
        self.sock = socket.create_connection(("127.0.0.1", port), 10)
        self.sock.settimeout(TIMEOUT)
        self.f = self.sock.makefile("rb")
        self.next_id = 1

    def close(self):
        try:
            self.f.close()
        except Exception:
            pass
        try:
            self.sock.close()
        except Exception:
            pass

    def req(self, method, params=None, timeout=TIMEOUT):
        rid = self.next_id
        self.next_id += 1
        o = {"id": rid, "method": method, "key": KEY_G}
        if params:
            o["params"] = params
        self.sock.sendall(json.dumps(o, ensure_ascii=False).encode()
                          + b"\n")
        old = self.sock.gettimeout()
        self.sock.settimeout(timeout)
        try:
            line = self.f.readline()
        finally:
            self.sock.settimeout(old)
        if not line:
            raise OSError("control channel EOF")
        # strict=False: list labels carry a raw \x07 marker
        return json.loads(line, strict=False)


KEY_G = ""   # per-instance auth key, set per launch


class App:
    """One abinova instance with the control channel up.

    The suite is expected to already run inside an X server with
    XTest and a private session bus (the __main__ re-exec arranges
    xvfb-run -a dbus-run-session)."""

    def __init__(self, args):
        self.args = args
        self.proc = None
        self.chan = None
        self.keypath = os.path.join(
            args.workdir, "config", "abinova",
            f"control-key.local-{args.port}")
        self.log = None

    def launch(self):
        env = dict(os.environ)
        env["GDK_BACKEND"] = "x11"
        env.pop("WAYLAND_DISPLAY", None)
        env["G_APPLICATION_NON_UNIQUE"] = "1"
        env["GTK_USE_PORTAL"] = "0"
        env["NO_AT_BRIDGE"] = "1"
        env["XDG_CONFIG_HOME"] = os.path.join(self.args.workdir, "config")
        env["XDG_DATA_HOME"] = os.path.join(self.args.workdir, "data")
        env["XDG_CACHE_HOME"] = os.path.join(self.args.workdir, "cache")
        # dropped autosave/crash-recovery state between cases
        shutil.rmtree(os.path.join(self.args.workdir, "config",
                                   "abinova", "autosave"),
                      ignore_errors=True)
        if os.path.exists(self.keypath):
            os.remove(self.keypath)
        self.log = open(os.path.join(self.args.workdir, "app.log"),
                        "wb", buffering=0)
        self.proc = subprocess.Popen(
            [self.args.bin, f"--control={self.args.port}"],
            env=env, stdout=self.log, stderr=self.log)
        self._await_ready()

    def _await_ready(self):
        deadline = time.monotonic() + 40
        while time.monotonic() < deadline:
            if self.proc.poll() is not None:
                raise RuntimeError(
                    f"app exited at launch rc={self.proc.returncode}")
            if os.path.exists(self.keypath):
                break
            time.sleep(0.1)
        else:
            raise RuntimeError("key file never appeared")
        global KEY_G
        KEY_G = open(self.keypath).read().strip()
        deadline = time.monotonic() + 40
        while True:
            try:
                self.chan = Channel(self.args.port, KEY_G)
                obj = self.chan.req("ping")
                if obj.get("result") == "pong":
                    break
                self.chan.close()
            except (OSError, socket.timeout):
                pass
            if self.proc.poll() is not None:
                raise RuntimeError("app died during connect")
            if time.monotonic() > deadline:
                raise RuntimeError("control channel never answered")
            time.sleep(0.3)
        # the channel can be up before the frame's view exists
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            try:
                obj = self.chan.req("document.inspect")
                if "result" in obj:
                    return
            except (OSError, socket.timeout):
                pass
            if self.proc.poll() is not None:
                raise RuntimeError("app died before view was ready")
            time.sleep(0.3)
        raise RuntimeError("frame view never became ready")

    def quit(self):
        if self.chan:
            try:
                self.chan.req("app.quit",
                              {"params": {"force": True}}, timeout=5)
            except Exception:
                pass
            self.chan.close()
            self.chan = None
        if self.proc and self.proc.poll() is None:
            try:
                self.proc.wait(timeout=8)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                try:
                    self.proc.wait(timeout=10)
                except Exception:
                    pass
        if self.log:
            try:
                self.log.close()
            except Exception:
                pass
            self.log = None


class Ctx:
    """Per-case helpers bound to the live channel."""

    def __init__(self, chan):
        self.ch = chan
        self.checks = 0
        self.fails = 0

    # -- actions -------------------------------------------------
    def key(self, spec):
        """ui.key: 'Return', 'a', 'ctrl+a', 'shift+Tab', ..."""
        r = self.ch.req("ui.key", {"key": spec})
        assert "result" in r, f"ui.key {spec!r}: {r}"
        time.sleep(SETTLE)

    def keys(self, *specs):
        for s in specs:
            self.key(s)

    def text(self, s):
        """ui.text (insertData) — bulk text insert at the caret."""
        r = self.ch.req("ui.text", {"text": s})
        assert "result" in r, f"ui.text {s!r}: {r}"
        time.sleep(SETTLE)

    def type_keys(self, s):
        """Type a string through the real XTest key path."""
        for c in s:
            self.key(c if c != " " else "space")

    def execute(self, command, data=None):
        p = {"command": command}
        if data is not None:
            p["data"] = data
        r = self.ch.req("engine.execute", p)
        assert "result" in r, f"execute {command!r}: {r}"
        time.sleep(SETTLE)

    # -- observation ---------------------------------------------
    def doc_text(self):
        return self.ch.req("document.text").get(
            "result", {}).get("text")

    def inspect(self):
        return self.ch.req("document.inspect").get("result", {})

    def fmt_report(self):
        return self.ch.req("document.formatinspect").get(
            "result", {}).get("report", "")

    def wait_text(self, expected, timeout=5.0):
        """Poll document.text until it equals expected."""
        deadline = time.monotonic() + timeout
        last = None
        while time.monotonic() < deadline:
            last = self.doc_text()
            if last == expected:
                return last
            time.sleep(0.15)
        return last

    # -- assertions -----------------------------------------------
    def ok(self, cond, label, detail=""):
        self.checks += 1
        if cond:
            eprint(f"    ok {self.checks:2d} {label}")
        else:
            self.fails += 1
            eprint(f"  FAIL {self.checks:2d} {label} {detail}")

    def eq(self, got, want, label):
        self.ok(got == want, label, f"(got {got!r}, want {want!r})")


# ------------------------------------------------------------------
# cases — one fresh app per case; see docs/typing-parity.md
# ------------------------------------------------------------------

def t_type_inserts_at_caret(c):
    c.text("hello")
    c.eq(c.wait_text("hello"), "hello", "insertData inserts text")
    i = c.inspect()
    c.eq(i.get("words"), 1, "word count")
    c.eq(i.get("chars"), 5, "char count")
    c.eq(i.get("paras"), 1, "para count")
    c.text(" world")
    c.eq(c.wait_text("hello world"), "hello world",
         "appends at caret")


def t_key_letters_insert(c):
    c.type_keys("abc")
    c.eq(c.wait_text("abc"), "abc",
         "XTest letters land in the document")
    i = c.inspect()
    c.eq(i.get("chars"), 3, "char count")


def t_enter_splits_at_eol(c):
    c.text("ab")
    c.key("Return")
    c.text("cd")
    c.eq(c.wait_text("ab\ncd"), "ab\ncd", "Enter splits paragraphs")
    c.eq(c.inspect().get("paras"), 2, "para count 2")


def t_enter_mid_word(c):
    c.text("hello")
    c.keys("Left", "Left")
    c.key("Return")
    c.eq(c.wait_text("hel\nlo"), "hel\nlo",
         "Enter mid-word splits at caret")


def t_enter_at_bol(c):
    c.text("abc")
    c.key("Home")
    c.key("Return")
    c.eq(c.wait_text("\nabc"), "\nabc",
         "Enter at BOL opens an empty paragraph before")


def t_backspace_deletes_char(c):
    c.text("abc")
    c.key("BackSpace")
    c.eq(c.wait_text("ab"), "ab", "BackSpace deletes left char")


def t_backspace_docstart_noop(c):
    before = c.inspect().get("point")
    c.key("BackSpace")
    c.eq(c.doc_text(), "", "BackSpace at doc start is a no-op")
    c.eq(c.inspect().get("point"), before, "caret unchanged")


def t_backspace_joins_paragraphs(c):
    c.text("ab")
    c.key("Return")
    c.text("cd")
    c.key("Home")
    c.key("BackSpace")
    c.eq(c.wait_text("abcd"), "abcd",
         "BackSpace at BOL joins with previous paragraph")
    c.eq(c.inspect().get("paras"), 1, "para count back to 1")


def t_delete_forward(c):
    c.text("abc")
    c.key("Home")
    c.key("Delete")
    c.eq(c.wait_text("bc"), "bc", "Delete removes the char right of caret")


def t_tab_inserts_tabchar(c):
    c.text("a")
    c.key("Tab")
    c.text("b")
    c.eq(c.wait_text("a\tb"), "a\tb",
         "Tab in body text inserts a tab character")


def t_select_all_type_replaces(c):
    c.text("hello")
    c.key("ctrl+a")
    c.text("bye")
    c.eq(c.wait_text("bye"), "bye",
         "typing over a selection replaces it")


def t_select_type_undo_atomic(c):
    c.text("hello")
    c.key("ctrl+a")
    c.text("bye")
    c.key("ctrl+z")
    c.eq(c.wait_text("hello"), "hello",
         "one undo restores the replaced text atomically")
    c.key("ctrl+y")
    c.eq(c.wait_text("bye"), "bye", "redo replays the replace")


def t_typed_run_coalesces_undo(c):
    # contiguous same-format keystroke inserts coalesce into a single
    # undo record (pt_PieceTable::_canCoalesceInsertSpan); the first
    # insert into a fresh empty document forms its own record, so the
    # run "hello" undoes as ['ello'] then ['h']
    c.type_keys("hello")
    c.eq(c.wait_text("hello"), "hello", "letters typed")
    c.key("ctrl+z")
    c.eq(c.wait_text("h"), "h",
         "undo 1 removes the coalesced run in one step")
    c.key("ctrl+z")
    c.eq(c.wait_text(""), "", "undo 2 removes the first char's record")
    c.keys("ctrl+y", "ctrl+y")
    c.eq(c.wait_text("hello"), "hello", "redo restores the run")


def t_bulk_insert_undo_steps(c):
    # each ui.text/insertData call is its own undo step (user-atomic
    # glob) — consecutive bulk inserts do not coalesce
    c.text("hel")
    c.text("lo")
    c.eq(c.wait_text("hello"), "hello", "two bulk inserts landed")
    c.key("ctrl+z")
    c.eq(c.wait_text("hel"), "hel", "undo 1 removes only the last insert")
    c.key("ctrl+z")
    c.eq(c.wait_text(""), "", "undo 2 removes the first insert")


def t_smart_quotes(c):
    c.text('say "hi" ok')
    c.eq(c.wait_text("say “hi” ok"), "say “hi” ok",
         'straight quotes promoted to “smart” quotes as typed')


def t_smart_quotes_nesting_and_delimiters(c):
    # wordcraft pin: nested quotes and opening delimiters
    c.text("\"'nested'\" '\"inner\"' (\"'parens'\")")
    c.eq(c.wait_text("“‘nested’” ‘“inner”’ (“‘parens’”)"),
         "“‘nested’” ‘“inner”’ (“‘parens’”)",
         "nested quotes alternate parity, '(' opens")
    # Empty quotes preserve open then close
    c.execute("selectAll")
    c.execute("delRight")
    c.text("\"\" ''")
    c.eq(c.wait_text("“” ‘’"), "“” ‘’",
         "empty quotes open then close")
    # Quote opens after guillemets and low quotes
    c.execute("selectAll")
    c.execute("delRight")
    c.text("«'french'» „'german'“")
    c.eq(c.wait_text("«‘french’» „‘german’“"),
         "«‘french’» „‘german’“",
         "' opens after « and „")


def t_smart_quotes_hebrew_logical_order(c):
    # wordcraft #1670: the decision uses the logical predecessor, so a
    # closing quote after a Hebrew letter is ” even though it displays
    # before the opening one; inside an acronym it's ” not gershayim.
    c.text("הוא אמר \"שלום\" לי")
    c.eq(c.wait_text("הוא אמר “שלום” לי"), "הוא אמר “שלום” לי",
         "Hebrew sentence quotes open/close")
    c.execute("selectAll")
    c.execute("delRight")
    c.text("\"שלום\"")
    c.eq(c.wait_text("“שלום”"), "“שלום”", "RTL-only quoted word")
    c.execute("selectAll")
    c.execute("delRight")
    c.text("צה\"ל ")
    c.eq(c.wait_text("צה”ל "), "צה”ל ", "acronym gets ” not gershayim")
    c.execute("selectAll")
    c.execute("delRight")
    c.text("ג'ירפה 'כן'")
    c.eq(c.wait_text("ג’ירפה ‘כן’"), "ג’ירפה ‘כן’",
         "' after letter closes; after space opens")


def t_list_enter_continues(c):
    c.execute("doNumbers")
    c.text("one")
    c.key("Return")
    c.text("two")
    c.eq(c.wait_text(LIST_MARK + "one\n" + LIST_MARK + "two"),
         LIST_MARK + "one\n" + LIST_MARK + "two",
         "Enter on a list item continues the list")


def t_list_enter_empty_stops(c):
    c.execute("doNumbers")
    c.text("one")
    c.key("Return")
    c.key("Return")
    c.text("tail")
    c.eq(c.wait_text(LIST_MARK + "one\ntail"),
         LIST_MARK + "one\ntail",
         "Enter on an empty list item stops the list")


def t_list_tab_demotes_shift_promotes(c):
    c.execute("doNumbers")
    c.text("one")
    c.key("Return")
    rep = c.fmt_report()
    c.ok("level 1" in rep, "new item starts at level 1",
         f"(report context: {rep[-120:]!r})")
    c.key("Tab")
    rep = c.fmt_report()
    c.ok("level 2" in rep, "Tab demotes the list item",
         f"(report context: {rep[-120:]!r})")
    c.eq(c.doc_text(), LIST_MARK + "one\n" + LIST_MARK,
         "demote does not alter document text")
    c.key("shift+Tab")
    rep = c.fmt_report()
    # gap vs Word: Shift+Tab promote is unbound outside tables —
    # pinned so the absence is deliberate
    c.ok("level 2" in rep, "Shift+Tab does not promote (documented gap)",
         f"(report context: {rep[-120:]!r})")


def t_table_autoformat(c):
    c.text("+---+---+")
    c.key("Return")
    c.eq(c.wait_text("\n\n"), "\n\n",
         "+---+---+ Enter becomes a 2-column table")
    deadline = time.monotonic() + 5
    rep = ""
    while time.monotonic() < deadline:
        rep = c.fmt_report()
        if "table" in rep.lower():
            break
        time.sleep(0.2)
    c.ok("table" in rep.lower(), "caret context is a table cell",
         f"(report tail: {rep[-160:]!r})")


def t_rand_expands(c):
    c.text("=rand(1,2)")
    c.key("Return")
    deadline = time.monotonic() + 5
    t = ""
    while time.monotonic() < deadline:
        t = c.doc_text() or ""
        if "quick brown fox" in t:
            break
        time.sleep(0.2)
    c.ok("quick brown fox" in t, "=rand() expands to sample text",
         f"(text: {t[:80]!r})")
    c.ok("=rand" not in t, "command line is consumed",
         f"(text: {t[:80]!r})")
    c.ok((c.inspect().get("words") or 0) > 10, "two sentences landed")


def t_tab_indents_paragraph_selection(c):
    # W31 / wordcraft #4291: Tab on a selection covering a whole
    # paragraph indents it by one 1/2" step instead of typing a tab
    c.text("hello world")
    c.key("ctrl+a")
    c.key("Tab")
    c.eq(c.doc_text(), "hello world",
         "Tab on selected paragraph types no tab character")
    rep = c.fmt_report()
    c.ok("margin-left" in rep and "0.5" in rep,
         "paragraph gains a 0.5in left indent",
         f"(report tail: {rep[-160:]!r})")


def t_tab_indents_multi_paragraph_selection(c):
    c.text("one")
    c.key("Return")
    c.text("two")
    c.key("ctrl+a")
    c.key("Tab")
    c.eq(c.doc_text(), "one\ntwo",
         "Tab on multi-paragraph selection types no tab")
    c.key("ctrl+Home")
    rep1 = c.fmt_report()
    c.ok("0.5" in rep1, "first paragraph indented",
         f"(report tail: {rep1[-160:]!r})")
    c.key("Down")
    rep2 = c.fmt_report()
    c.ok("0.5" in rep2, "second paragraph indented",
         f"(report tail: {rep2[-160:]!r})")


def t_tab_partial_selection_still_tabs(c):
    # a selection inside one paragraph still gets the tab character
    c.text("hello")
    c.key("Home")
    c.keys("shift+Right", "shift+Right", "shift+Right", "shift+Right")
    c.key("Tab")
    c.eq(c.wait_text("\to"), "\to",
         "partial selection is replaced by a tab character")


def t_alt_numpad_types_character(c):
    # W31 / wordcraft #1146: Alt+numpad decimal code entry — ui.key
    # holds the first segment's modifiers across the comma sequence
    c.key("alt+KP_0,KP_6,KP_5")
    c.eq(c.wait_text("A"), "A", "Alt+065 types 'A'")
    c.key("alt+KP_8,KP_3,KP_6,KP_4")
    c.eq(c.wait_text("A€"), "A€", "Alt+8364 types '€'")
    c.key("alt+KP_9,KP_9,KP_9,KP_9,KP_9,KP_9,KP_9,KP_9")
    c.eq(c.wait_text("A€"), "A€",
         "code past Unicode types nothing")


def t_no_auto_capitalise(c):
    # gap vs Word: Abinova does not auto-capitalise sentence starts —
    # pinned so the absence is a deliberate, tested fact
    c.text("hello. next sentence")
    c.eq(c.wait_text("hello. next sentence"),
         "hello. next sentence",
         "no auto-capitalise on sentence starts (documented gap)")


def t_autocorrect(c):
    # W59: as-you-type replacement — "teh" becomes "the" on the
    # word-boundary space (trigger is the insert's last char, so the
    # space has to land as its own insert)
    c.text("teh ")
    c.text("quick")
    c.eq(c.wait_text("the quick"), "the quick",
         "autocorrect 'teh' -> 'the' on word boundary")


def t_alt_x_toggle(c):
    # W61 / wordcraft text.toggleUnicode: hex code <-> char
    c.text("03B1")
    c.key("alt+x")
    c.eq(c.wait_text("α"), "α", "03B1 Alt+X -> α")
    c.key("alt+x")
    c.eq(c.wait_text("03B1"), "03B1", "α Alt+X -> 03B1")


def t_alt_x_prefix_and_selection(c):
    # U+ prefix goes with the digits; a selection converts too
    c.text("U+00E9")
    c.key("alt+x")
    c.eq(c.wait_text("é"), "é", "U+00E9 Alt+X -> é")
    c.execute("selectAll")
    c.execute("delRight")
    c.text("0041")
    c.key("ctrl+a")
    c.key("alt+x")
    c.eq(c.wait_text("A"), "A", "selected 0041 -> A")
    c.key("ctrl+a")
    c.key("alt+x")
    c.eq(c.wait_text("0041"), "0041", "selected A -> 0041")


def t_alt_x_edges(c):
    # longest valid code wins: 110000 > U+10FFFF -> tries 10000
    c.text("110000")
    c.key("alt+x")
    c.eq(c.wait_text("1𐀀"), "1𐀀", "110000 -> '1' + U+10000")
    # a non-hex char bounds the digit scan
    c.execute("selectAll")
    c.execute("delRight")
    c.text("x41")
    c.key("alt+x")
    c.eq(c.wait_text("xA"), "xA", "x41 -> xA")
    # nothing before the caret: no-op
    c.execute("selectAll")
    c.execute("delRight")
    c.key("alt+x")
    c.eq(c.wait_text(""), "", "Alt+X at doc start is a no-op")


def t_move_paragraph(c):
    # W61 / wordcraft para.moveUp|Down: Alt+Shift+Up/Down swap the
    # touched paragraphs with their neighbour
    c.text("one")
    c.key("Return")
    c.text("two")
    c.key("alt+shift+Up")
    c.eq(c.wait_text("two\none"), "two\none",
         "Alt+Shift+Up moves the paragraph up")
    # the caret rides the moved paragraph; move it back down
    c.key("ctrl+Home")
    c.key("alt+shift+Down")
    c.eq(c.wait_text("one\ntwo"), "one\ntwo",
         "Alt+Shift+Down moves the paragraph down")
    c.key("ctrl+Home")
    c.key("alt+shift+Up")
    c.eq(c.wait_text("one\ntwo"), "one\ntwo",
         "Alt+Shift+Up at the top is refused")


def t_move_paragraph_in_cell(c):
    # same command inside a table cell: block siblings swap within
    # the cell (wordcraft: paragraphs move in their own container)
    c.text("+---+---+")
    c.key("Return")
    rep = ""
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        rep = c.fmt_report()
        if "table" in rep.lower():
            break
        time.sleep(0.2)
    c.text("a1")
    c.key("Return")
    c.text("a2")
    c.key("alt+shift+Up")
    c.eq(c.wait_text("a2\na1\n\n"), "a2\na1\n\n",
         "Alt+Shift+Up swaps paragraphs inside the cell")
    c.key("alt+shift+Down")
    c.eq(c.wait_text("a1\na2\n\n"), "a1\na2\n\n",
         "Alt+Shift+Down swaps them back")


def t_ctrl_tab_in_cell(c):
    # Ctrl+Tab types a real tab even where Tab moves to the next cell
    c.text("+---+---+")
    c.key("Return")
    rep = ""
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        rep = c.fmt_report()
        if "table" in rep.lower():
            break
        time.sleep(0.2)
    c.text("A")
    c.key("ctrl+Tab")
    c.text("B")
    c.eq(c.wait_text("A\tB\n\n"), "A\tB\n\n",
         "Ctrl+Tab inserts a tab character inside the cell")


def t_ctrl_alt_h_highlight(c):
    # W61 / wordcraft format.highlight toggle: Ctrl+Alt+H applies
    # the last-chosen colour (yellow in a fresh session) and removes
    # it again when the whole target already has it
    c.text("Alpha Beta")
    c.key("ctrl+a")
    c.key("ctrl+alt+h")
    rep = c.fmt_report()
    c.ok("bgcolor = ffff00" in rep, "Ctrl+Alt+H highlights yellow",
         f"(report tail: {rep[-160:]!r})")
    c.key("ctrl+alt+h")
    rep = c.fmt_report()
    c.ok("ffff00" not in rep and "transparent" in rep,
         "second Ctrl+Alt+H removes the highlight",
         f"(report tail: {rep[-160:]!r})")
    # an explicit pick becomes the toggled colour
    c.execute("colorBackTB", "0000ff")
    c.key("ctrl+alt+h")
    rep = c.fmt_report()
    c.ok("transparent" in rep and "0000ff" not in rep,
         "Ctrl+Alt+H removes the last-chosen colour",
         f"(report tail: {rep[-160:]!r})")
    c.key("ctrl+alt+h")
    rep = c.fmt_report()
    c.ok("bgcolor = 0000ff" in rep,
         "Ctrl+Alt+H reapplies the last-chosen colour",
         f"(report tail: {rep[-160:]!r})")


CASES = [
    ("type-inserts-at-caret", t_type_inserts_at_caret),
    ("key-letters-insert", t_key_letters_insert),
    ("enter-splits-at-eol", t_enter_splits_at_eol),
    ("enter-mid-word", t_enter_mid_word),
    ("enter-at-bol", t_enter_at_bol),
    ("backspace-deletes-char", t_backspace_deletes_char),
    ("backspace-docstart-noop", t_backspace_docstart_noop),
    ("backspace-joins-paragraphs", t_backspace_joins_paragraphs),
    ("delete-forward-char", t_delete_forward),
    ("tab-inserts-tabchar", t_tab_inserts_tabchar),
    ("select-all-type-replaces", t_select_all_type_replaces),
    ("select-type-undo-atomic", t_select_type_undo_atomic),
    ("typed-run-coalesces-undo", t_typed_run_coalesces_undo),
    ("bulk-insert-undo-steps", t_bulk_insert_undo_steps),
    ("smart-quotes", t_smart_quotes),
    ("smart-quotes-nesting", t_smart_quotes_nesting_and_delimiters),
    ("smart-quotes-hebrew", t_smart_quotes_hebrew_logical_order),
    ("list-enter-continues", t_list_enter_continues),
    ("list-enter-empty-stops", t_list_enter_empty_stops),
    ("list-tab-demotes", t_list_tab_demotes_shift_promotes),
    ("table-autoformat", t_table_autoformat),
    ("rand-expands", t_rand_expands),
    ("tab-indents-paragraph-selection", t_tab_indents_paragraph_selection),
    ("tab-indents-multi-paragraph", t_tab_indents_multi_paragraph_selection),
    ("tab-partial-selection-tabs", t_tab_partial_selection_still_tabs),
    ("alt-numpad-character", t_alt_numpad_types_character),
    ("no-auto-capitalise", t_no_auto_capitalise),
    ("autocorrect", t_autocorrect),
    ("alt-x-toggle", t_alt_x_toggle),
    ("alt-x-prefix-selection", t_alt_x_prefix_and_selection),
    ("alt-x-edges", t_alt_x_edges),
    ("move-paragraph", t_move_paragraph),
    ("move-paragraph-in-cell", t_move_paragraph_in_cell),
    ("ctrl-tab-in-cell", t_ctrl_tab_in_cell),
    ("ctrl-alt-h-highlight", t_ctrl_alt_h_highlight),
]


def run_suite(args):
    only = re.compile(args.only) if args.only else None
    total_checks = 0
    total_fails = 0
    ran = 0
    for name, fn in CASES:
        if only and not only.search(name):
            continue
        ran += 1
        eprint(f"[case] {name}")
        app = App(args)
        try:
            app.launch()
        except Exception as ex:
            eprint(f"  INFRA FAIL: {ex}")
            return 2
        c = Ctx(app.chan)
        try:
            fn(c)
        except Exception as ex:
            c.fails += 1
            eprint(f"  EXCEPTION: {ex!r}")
        finally:
            app.quit()
        total_checks += c.checks
        total_fails += c.fails
        status = "PASS" if c.fails == 0 else "FAIL"
        eprint(f"  {status} ({c.checks - c.fails}/{c.checks})")
    eprint(f"[done] {ran} cases, "
           f"{total_checks - total_fails}/{total_checks} checks")
    return 0 if total_fails == 0 else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=os.path.join(REPO, "src", "abinova"))
    ap.add_argument("--port", type=int, default=7961)
    ap.add_argument("--only", default=None,
                    help="regex filter on case names")
    ap.add_argument("--keep", action="store_true",
                    help="keep the temp workdir on exit")
    ap.add_argument("--child", action="store_true",
                    help="internal: already inside xvfb/dbus session")
    ap.add_argument("--workdir", default=None)
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    if not args.child:
        # re-exec inside a private X server + session bus so ui.key
        # XTest input has somewhere to land; needs:tool:xvfb-run
        if not shutil.which("xvfb-run"):
            if not os.environ.get("DISPLAY"):
                eprint("error: no xvfb-run and no DISPLAY — "
                       "XTest input impossible")
                return 2
            eprint("warning: no xvfb-run — using real DISPLAY "
                   f"{os.environ.get('DISPLAY')}")
            return run_inner(args)
        if not shutil.which("dbus-run-session"):
            eprint("error: no dbus-run-session")
            return 2
        env = dict(os.environ)
        env["TP_CHILD"] = "1"
        cmd = ["xvfb-run", "-a", "dbus-run-session", "--",
               sys.executable, os.path.abspath(__file__), "--child"]
        cmd += sys.argv[1:]
        return subprocess.call(cmd, env=env)
    return run_inner(args)


def run_inner(args):
    args.bin = os.path.abspath(args.bin)
    if not os.path.exists(args.bin):
        eprint(f"error: binary {args.bin} does not exist "
               "(cd src && make -j2)")
        return 2
    if args.workdir:
        workdir = args.workdir
        os.makedirs(workdir, exist_ok=True)
        return run_suite(args)
    workdir = tempfile.mkdtemp(prefix="typing-parity-")
    try:
        args.workdir = workdir
        return run_suite(args)
    finally:
        if not args.keep:
            shutil.rmtree(workdir, ignore_errors=True)
        else:
            eprint(f"[keep] workdir {workdir}")


if __name__ == "__main__":
    sys.exit(main())
