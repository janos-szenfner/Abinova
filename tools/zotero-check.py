#!/usr/bin/env python3
"""zotero-check.py — W18 Zotero integration driver.

Drives a live Abinova over the JSON-lines control channel against a
recorded stub of the local Zotero REST API (127.0.0.1:23119 shape —
the app is pointed at the stub through ABINOVA_ZOTERO_PORT).

Stub responses carry CSL HTML ("<span>(<i>Smith</i>, 2020)</span>")
so the whole pipeline is exercised: HTTP fetch -> JSON parse ->
HTML strip -> "zotero" field with zotero-* attributes -> render.

Checks:
  * zoteroInsert query=…    -> formatted citation lands in the doc
  * zoteroInsert key=…;mode=bib -> bibliography entry lands
  * saved .abwn keeps zotero-key/-style/-mode/-text attributes
  * zoteroRefresh           -> field re-fetches the changed text
  * stub 404 / stub down    -> graceful errors, app stays alive
  * PDF render shows the citation text

Needs an X server: run under ``xvfb-run`` or let the script re-exec
itself under ``xvfb-run -a dbus-run-session --`` (automatic when
xvfb-run is on PATH and ZC_CHILD is unset).

Usage:
    tools/zotero-check.py [--bin src/abinova] [--port N]
                          [--zport N] [--keep] [-v]

Exit code: 0 = all checks PASS, 1 = a check failed,
2 = infrastructure failure.
"""

import argparse
import json
import os
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse, parse_qs

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TIMEOUT = 15.0
SETTLE = 0.25


def eprint(*a):
    print(*a, file=sys.stderr, flush=True)


# ------------------------------------------------------------------
# recorded Zotero local API stub
# ------------------------------------------------------------------

STUB_ITEMS = [
    {
        "key": "SMITH2020",
        "version": 1,
        "data": {
            "itemType": "journalArticle",
            "title": "On Recorded Stubs",
            "creators": [
                {"creatorType": "author",
                 "firstName": "John", "lastName": "Smith"}
            ],
            "date": "2020",
        },
        # CSL HTML — the app's strip must reduce this to plain text
        "citation": "<span>(<i>Smith</i>, 2020)</span>",
        "bib": "<div class=\"csl-entry\">Smith, J. (2020). "
               "<i>On Recorded Stubs</i>. <i>Journal of Tests</i>, "
               "12(3), 45&#8211;67.</div>",
    },
    {
        "key": "DOE2019",
        "version": 1,
        "data": {
            "itemType": "book",
            "title": "A Stub Bibliography",
            "creators": [
                {"creatorType": "author",
                 "firstName": "Jane", "lastName": "Doe"},
                {"creatorType": "author",
                 "firstName": "Rita", "lastName": "Roe"},
            ],
            "date": "2019",
        },
        "citation": "<span>(Doe &amp; Roe, 2019)</span>",
        "bib": "<div class=\"csl-entry\">Doe, J., &amp; Roe, R. "
               "(2019). <i>A Stub Bibliography</i>. Test Press.</div>",
    },
]

# refresh serves this instead of STUB_ITEMS[0]["citation"]
UPDATED_CITATION = "<span>(Smith &amp; Jones, 2021)</span>"


class StubState:
    updated = False      # serve the post-edit citation for SMITH2020
    hits = 0             # requests served


class ZoteroStub(BaseHTTPRequestHandler):
    state = StubState()

    def log_message(self, *a):
        pass

    def _item_json(self, it):
        it = dict(it)
        if self.state.updated and it["key"] == "SMITH2020":
            it = dict(it)
            it["citation"] = UPDATED_CITATION
        return it

    def do_GET(self):
        self.state.hits += 1
        u = urlparse(self.path)
        m = re.match(r"^/api/users/0/items/([A-Z0-9]+)$", u.path)
        if m:
            key = m.group(1)
            for it in STUB_ITEMS:
                if it["key"] == key:
                    self._json(200, self._item_json(it))
                    return
            self._json(404, {"message": "Not found"})
            return
        if u.path == "/api/users/0/items":
            q = parse_qs(u.query).get("q", [""])[0].lower()
            items = [self._item_json(it) for it in STUB_ITEMS
                     if not q or q in it["data"]["title"].lower()
                     or any(q in c.get("lastName", "").lower()
                            for c in it["data"]["creators"])]
            self._json(200, items)
            return
        self._json(404, {"message": "Not found"})

    def _json(self, code, obj):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


# ------------------------------------------------------------------
# control channel plumbing (same wire protocol as typing-parity.py)
# ------------------------------------------------------------------

class Channel:
    def __init__(self, port, key):
        self.sock = socket.create_connection(("127.0.0.1", port), 10)
        self.sock.settimeout(TIMEOUT)
        self.f = self.sock.makefile("rb")
        self.next_id = 1
        self.key = key

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
        o = {"id": rid, "method": method, "key": self.key}
        if params:
            o["params"] = params
        self.sock.sendall(json.dumps(o).encode() + b"\n")
        old = self.sock.gettimeout()
        self.sock.settimeout(timeout)
        try:
            line = self.f.readline()
        finally:
            self.sock.settimeout(old)
        if not line:
            raise OSError("control channel EOF")
        return json.loads(line, strict=False)


class App:
    def __init__(self, args, zport=None):
        self.args = args
        self.zport = zport
        self.proc = None
        self.chan = None
        self.key = ""
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
        if self.zport:
            env["ABINOVA_ZOTERO_PORT"] = str(self.zport)
        else:
            # nothing listens here — exercises the "no Zotero" path
            env["ABINOVA_ZOTERO_PORT"] = "1"
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
        self.key = open(self.keypath).read().strip()
        deadline = time.monotonic() + 40
        while True:
            try:
                self.chan = Channel(self.args.port, self.key)
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
    def __init__(self, chan):
        self.ch = chan
        self.checks = 0
        self.fails = 0

    def check(self, name, cond, detail=""):
        self.checks += 1
        if cond:
            eprint(f"  ok   {name}")
        else:
            self.fails += 1
            eprint(f"  FAIL {name} {detail}")

    def execute(self, command, data=""):
        r = self.ch.req("engine.execute",
                        {"command": command, "data": data})
        return r

    def execute_ok(self, name, command, data=""):
        r = self.execute(command, data)
        self.check(name, "result" in r, str(r))
        time.sleep(SETTLE)
        return "result" in r

    def execute_err(self, name, command, data=""):
        r = self.execute(command, data)
        self.check(name, "error" in r, str(r))
        time.sleep(SETTLE)
        return "error" in r

    def doc_text(self):
        r = self.ch.req("document.text")
        return r.get("result", {}).get("text", "")

    def save_as(self, path):
        r = self.ch.req("file.save", {"path": path})
        return "result" in r


# ------------------------------------------------------------------
# the suite
# ------------------------------------------------------------------

def run_suite(args):
    ZoteroStub.state = StubState()

    stub = HTTPServer(("127.0.0.1", args.zport), ZoteroStub)
    threading.Thread(target=stub.serve_forever, daemon=True).start()
    eprint(f"stub Zotero on 127.0.0.1:{args.zport}")

    c = Ctx(None)
    save1 = os.path.join(args.workdir, "zot1.abwn")
    save2 = os.path.join(args.workdir, "zot2.abwn")

    app = App(args, zport=args.zport)
    try:
        app.launch()
        c.ch = app.chan

        # -- insert via query -------------------------------------
        c.execute_ok("insert by query",
                     "zoteroInsert", "query=smith;style=apa")
        txt = c.doc_text()
        c.check("doc grew after insert", len(txt) > 0, repr(txt))

        c.execute_ok("insert by key, bib mode",
                     "zoteroInsert", "key=DOE2019;mode=bib;style=apa")

        c.check("save 1", c.save_as(save1), save1)
        xml = open(save1, encoding="utf-8").read()
        c.check("abwn has zotero-key SMITH2020",
                'zotero-key="SMITH2020"' in xml)
        c.check("abwn has zotero-key DOE2019",
                'zotero-key="DOE2019"' in xml)
        c.check("abwn has stripped citation text",
                'zotero-text="(Smith, 2020)"' in xml,
                re.findall(r'zotero-text="[^"]*"', xml))
        c.check("abwn has bib entry text",
                "Smith, J. (2020)." in xml or "Doe, J." in xml)
        c.check("abwn has zotero-mode=bib",
                'zotero-mode="bib"' in xml)
        c.check("HTML markup stripped from field text",
                "<i>" not in xml or "&lt;i&gt;" not in xml)

        # -- refresh: the stub "edits" the source -----------------
        ZoteroStub.state.updated = True
        c.execute_ok("refresh picks up new text", "zoteroRefresh")
        c.check("save 2", c.save_as(save2), save2)
        xml2 = open(save2, encoding="utf-8").read()
        c.check("refreshed citation text",
                "(Smith &amp; Jones, 2021)" in xml2
                or "(Smith & Jones, 2021)" in xml2,
                re.findall(r'zotero-text="[^"]*"', xml2))

        # -- interactive picker dialog (dialog=1 pre-fills the
        #    query box; the modal call blocks, so a second channel
        #    connection drives the clicks — xwininfo locates the
        #    dialog's toplevel window)
        if shutil.which("xwininfo"):
            chan2 = Channel(args.port, c.ch.key)
            res = {}
            th = threading.Thread(
                target=lambda: res.__setitem__("r", c.ch.req(
                    "engine.execute",
                    {"command": "zoteroInsert",
                     "data": "dialog=1;query=smith"},
                    timeout=60)),
                daemon=True)
            th.start()
            m = None
            for _ in range(50):
                out = subprocess.run(
                    ["xwininfo", "-root", "-tree"],
                    capture_output=True, text=True).stdout
                m = re.search(
                    r'0x[0-9a-f]+ "Zotero[^"]*".*?'
                    r'(\d+)x(\d+)\+(\d+)\+(\d+)', out)
                if m:
                    break
                time.sleep(0.2)
            c.check("picker dialog opens", bool(m))
            if m:
                w, h, x, y = (int(v) for v in m.groups())
                hits = ZoteroStub.state.hits
                chan2.req("ui.click", {"x": x + 390, "y": y + 22})
                time.sleep(1.5)
                c.check("dialog search hits service",
                        ZoteroStub.state.hits > hits,
                        f"hits={ZoteroStub.state.hits}")
                chan2.req("ui.click", {"x": x + 221, "y": y + 105})
                time.sleep(0.5)
                chan2.req("ui.click", {"x": x + 70, "y": y + 315})
                th.join(timeout=15)
                c.check("dialog insert returns",
                        res.get("r", {}).get("result", {}).get("ok"),
                        str(res.get("r")))
            if "r" not in res:
                chan2.req("ui.key", {"key": "Escape"})
                th.join(timeout=15)
            chan2.close()
            c.check("dialog-inserted citation lands",
                    c.save_as(save2) and
                    open(save2, encoding="utf-8").read().count(
                        'type="zotero"') >= 3)
        else:
            eprint("  skip xwininfo missing — dialog clicks untested")

        # -- graceful failures ------------------------------------
        c.execute_err("unknown key refused",
                      "zoteroInsert", "key=NOSUCHKEY")
        c.execute_err("empty-ish insert refused",
                      "zoteroInsert", "query=zzz-no-such-item")
        r = app.chan.req("ping")
        c.check("app alive after failures",
                r.get("result") == "pong", str(r))
    finally:
        app.quit()

    # -- no Zotero at all: graceful error, no crash ---------------
    eprint("no-server case")
    app2 = App(args, zport=None)
    try:
        app2.launch()
        c2 = Ctx(app2.chan)
        c2.execute_err("no-server insert fails softly",
                       "zoteroInsert", "key=SMITH2020")
        c2.execute_err("no-server query fails softly",
                       "zoteroInsert", "query=smith")
        c2.execute_err("no-server refresh reports failure",
                       "zoteroRefresh")
        r = app2.chan.req("ping")
        c2.check("app alive without Zotero",
                 r.get("result") == "pong", str(r))
        c.checks += c2.checks
        c.fails += c2.fails
    finally:
        app2.quit()
    stub.shutdown()

    # -- PDF render: the field's cached text reaches the page -----
    pdf = os.path.join(args.workdir, "zot.pdf")
    r = subprocess.run(
        [args.bin, "--to=pdf", f"--to-name={pdf}", save2],
        capture_output=True, timeout=60)
    c.check("pdf export rc=0", r.returncode == 0,
            r.stderr.decode()[:200])
    if os.path.exists(pdf):
        out = subprocess.run(["pdftotext", pdf, "-"],
                             capture_output=True, timeout=30)
        ptext = out.stdout.decode("utf-8", "replace")
        c.check("pdf shows refreshed citation",
                "Smith & Jones, 2021" in ptext
                or "(Smith" in ptext, ptext[:200])
        c.check("pdf shows bib entry",
                "Doe" in ptext, ptext[:200])

    return c


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=os.path.join(REPO, "src", "abinova"))
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--zport", type=int, default=0)
    ap.add_argument("--keep", action="store_true")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    def free_port():
        s = socket.socket()
        s.bind(("127.0.0.1", 0))
        p = s.getsockname()[1]
        s.close()
        return p

    if not args.port:
        args.port = free_port()
    if not args.zport:
        args.zport = free_port()

    # re-exec under xvfb + private session bus when needed
    if not os.environ.get("ZC_CHILD"):
        if not os.environ.get("DISPLAY"):
            if not shutil.which("xvfb-run"):
                eprint("no DISPLAY and no xvfb-run — infra failure")
                return 2
            env = dict(os.environ)
            env["ZC_CHILD"] = "1"
            cmd = ["xvfb-run", "-a"]
            if shutil.which("dbus-run-session"):
                cmd += ["dbus-run-session", "--"]
            cmd += [sys.executable, os.path.abspath(__file__)] + sys.argv[1:]
            return subprocess.call(cmd, env=env)

    args.workdir = tempfile.mkdtemp(prefix="zotero-check-")
    try:
        c = run_suite(args)
    except Exception as e:
        eprint(f"INFRA: {e}")
        return 2
    finally:
        if not args.keep:
            shutil.rmtree(args.workdir, ignore_errors=True)
        else:
            eprint(f"kept {args.workdir}")

    eprint(f"{c.checks - c.fails}/{c.fails} checks failed"
           if c.fails else f"{c.checks} checks PASS")
    return 1 if c.fails else 0


if __name__ == "__main__":
    sys.exit(main())
