#!/usr/bin/env python3
"""hostile-params.py — W08 hostile-params fuzz driver.

Walks every edit method the running app registers (engine.commands)
and fires it through engine.execute with hostile params.data payloads:
absent, empty, huge, non-UTF-8, NUL-bearing, format-string, traversal,
numeric-edge and parser-injection byte strings.  The app runs on a
loopback control channel under xvfb with --automation-*-root
confinement, so file-path commands are exercised both inside and
outside their allowed roots.

A request whose reply never arrives is a hang; a process that dies on
a signal is a crash — both are logged with the exact command+payload,
the app is restarted, and the sweep resumes at the next combo.
Commands that exit the app *cleanly* (closeWindowX, querySaveAndExit
and friends) are deferred to the end of the queue so their restarts
don't disturb the main sweep.

Protocol-level hostility is covered first: unauthenticated requests,
non-JSON lines, binary garbage, >1 MiB lines and hostile command names
all have to produce an error reply (or a clean connection drop) — not
a crash.

Usage:
    tools/hostile-params.py [--bin src/abinova] [--port N]
                            [--doc test/wp/cov07/rich.abw]
                            [--timeout SECS] [--restart-every N]
                            [--progress FILE] [--resume]
                            [--only REGEX] [--skip REGEX]
                            [--report FILE] [-v]

Exit code: 0 = sweep clean (no crashes/hangs), 1 = findings,
2 = infrastructure failure (could not launch/connect).
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

# Commands that legitimately destroy the frame/app — each still gets
# the full payload battery, but they run after the main sweep so a
# clean exit only costs the restart it needs.
DEFER_RE = re.compile(r"(quit|exit|close|activateWindow)", re.I)

TIMEOUT = 12.0        # reply deadline per execute
ESCAPE_AFTER = 2.0    # send an XTest Escape this far into a wait


def eprint(*a):
    print(*a, file=sys.stderr, flush=True)


def wire_escape(raw: bytes) -> bytes:
    """Encode decoded payload bytes as the inside of a JSON-ish string
    for the control channel's _json_str parser (escapes: n t r and
    char-for-char otherwise).  Raw newline/CR/quote would corrupt the
    line protocol, so they go through escapes; every other byte —
    including NUL and invalid UTF-8 — goes verbatim."""
    out = bytearray()
    for b in raw:
        if b == 0x22:            # "
            out += b'\\"'
        elif b == 0x5C:          # backslash
            out += b'\\\\'
        elif b == 0x0A:
            out += b'\\n'
        elif b == 0x0D:
            out += b'\\r'
        elif b == 0x09:
            out += b'\\t'
        else:
            out.append(b)
    return bytes(out)


class Channel:
    """One authenticated control-channel connection."""

    def __init__(self, port, key):
        self.sock = socket.create_connection(("127.0.0.1", port), 10)
        self.key = key
        self.next_id = 1
        self.buf = b""
        self.pending = {}        # id -> parsed reply, out of order

    def close(self):
        try:
            self.sock.close()
        except Exception:
            pass

    def send_line(self, line: bytes):
        self.sock.sendall(line + b"\n")

    def read_line(self, deadline):
        """One raw reply line; b'' on orderly EOF.
        Raises socket.timeout on deadline, OSError on reset."""
        while b"\n" not in self.buf:
            left = deadline - time.monotonic()
            if left <= 0:
                raise socket.timeout("reply deadline")
            self.sock.settimeout(left)
            chunk = self.sock.recv(1 << 20)
            if not chunk:
                return b""
            self.buf += chunk
        line, self.buf = self.buf.split(b"\n", 1)
        return line

    def read_reply(self, rid, deadline):
        """Parsed reply for rid; stashes other ids in pending.
        Returns ('reply',obj) | ('eof',None) | ('timeout',None)."""
        if rid in self.pending:
            return ("reply", self.pending.pop(rid))
        try:
            while True:
                reply = self.read_line(deadline)
                if reply == b"":
                    return ("eof", None)
                try:
                    obj = json.loads(reply)
                except Exception:
                    continue
                oid = obj.get("id")
                if oid == rid:
                    return ("reply", obj)
                if oid is not None:
                    self.pending[oid] = obj
        except socket.timeout:
            return ("timeout", None)

    def request(self, method, extra: dict, timeout=TIMEOUT):
        rid = self.next_id
        self.next_id += 1
        req = {"id": rid, "method": method, "key": self.key}
        req.update(extra)
        self.send_line(json.dumps(req).encode())
        return self.read_reply(rid, time.monotonic() + timeout)


class App:
    """One abinova instance under xvfb with the control channel up."""

    def __init__(self, args, workdir, docpath):
        self.args = args
        self.workdir = workdir
        self.docpath = docpath
        self.proc = None
        self.chan = None
        self.key = ""
        self.serial = 0
        self.log = None
        self.keypath = os.path.join(
            workdir, "config", "abinova",
            f"control-key.local-{args.port}")

    def launch(self):
        self.teardown()
        env = dict(os.environ)
        env["GDK_BACKEND"] = "x11"
        env.pop("WAYLAND_DISPLAY", None)
        env["G_APPLICATION_NON_UNIQUE"] = "1"
        # keep the private-bus launches lean: no portal services and
        # no AT-SPI bridge to spin up on every (re)start
        env["GTK_USE_PORTAL"] = "0"
        env["NO_AT_BRIDGE"] = "1"
        env["XDG_CONFIG_HOME"] = os.path.join(self.workdir, "config")
        env["XDG_DATA_HOME"] = os.path.join(self.workdir, "data")
        env["XDG_CACHE_HOME"] = os.path.join(self.workdir, "cache")
        os.makedirs(os.path.dirname(self.keypath), exist_ok=True)
        if os.path.exists(self.keypath):
            os.remove(self.keypath)
        # crash-handler .saved files get re-imported by
        # recoverAutosavedDocs at every launch; a fuzz-killed doc can
        # re-crash recovery forever (crash -> save .saved -> recover ->
        # crash).  The app now quarantines repeat offenders via the
        # .recovering sentinel, but starting each launch with a clean
        # autosave dir keeps the sweep measuring command handling, not
        # recovery loops.
        shutil.rmtree(os.path.join(self.workdir, "config", "abinova",
                                   "autosave"), ignore_errors=True)
        cmd = []
        if shutil.which("xvfb-run"):
            cmd += ["xvfb-run", "-a"]
        # GtkApplication is single-instance per app-id on the session
        # bus: a second launch on the same bus forwards to the first
        # and exits before the control channel ever opens.  A private
        # bus per launch keeps every (re)start a real, reachable
        # process (G_APPLICATION_NON_UNIQUE alone proved unreliable).
        if shutil.which("dbus-run-session"):
            cmd += ["dbus-run-session", "--"]
        cmd += [self.args.bin,
                f"--control={self.args.port}",
                f"--automation-read-root={self.workdir}",
                f"--automation-write-root={self.workdir}",
                self.docpath]
        self.serial += 1
        self.log = open(
            os.path.join(self.workdir, f"app-{self.serial:03d}.log"),
            "wb", buffering=0)
        # start_new_session: xvfb-run is a shell wrapper — signals to
        # the Popen pid reach it, not the real app.  A private process
        # group lets teardown kill the whole tree (xvfb + abinova).
        self.proc = subprocess.Popen(
            cmd, env=env, stdout=self.log, stderr=self.log,
            start_new_session=True)
        try:
            self._await_ready()
        except Exception:
            self.teardown(graceful=False)
            raise

    def _await_ready(self):
        deadline = time.monotonic() + 25
        while time.monotonic() < deadline:
            if self.proc.poll() is not None:
                raise RuntimeError(
                    f"app exited at launch rc={self.proc.returncode}")
            if os.path.exists(self.keypath):
                break
            time.sleep(0.1)
        else:
            raise RuntimeError("key file never appeared")
        with open(self.keypath) as fh:
            self.key = fh.read().strip()
        deadline = time.monotonic() + 30
        while True:
            try:
                self.chan = Channel(self.args.port, self.key)
                st, obj = self.chan.request("ping", {})
                if st == "reply" and obj.get("result") == "pong":
                    break
            except (OSError, socket.timeout):
                pass
            if self.proc.poll() is not None:
                raise RuntimeError("app died during connect")
            if time.monotonic() > deadline:
                raise RuntimeError("control channel never answered")
            time.sleep(0.2)
        # the channel can be up before the frame's view exists —
        # document commands would all bounce off "no view"
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            try:
                st, obj = self.chan.request("document.inspect", {})
                if st == "reply" and "result" in obj:
                    return
            except (OSError, socket.timeout):
                pass
            if self.proc.poll() is not None:
                raise RuntimeError("app died before view was ready")
            time.sleep(0.2)
        raise RuntimeError("frame view never became ready")

    def reconnect(self):
        try:
            ch = Channel(self.args.port, self.key)
            st, obj = ch.request("ping", {}, timeout=5)
            if st == "reply":
                if self.chan:
                    self.chan.close()
                self.chan = ch
                return True
            ch.close()
        except Exception:
            pass
        return False

    def alive(self):
        return self.proc is not None and self.proc.poll() is None

    def death(self):
        """None if running; ('signal',sig) | ('exit',rc)."""
        rc = self.proc.poll() if self.proc else -1
        if rc is None:
            return None
        if rc < 0:
            return ("signal", -rc)
        if rc >= 128:
            return ("signal", rc - 128)
        return ("exit", rc)

    def _kill_tree(self):
        try:
            os.killpg(os.getpgid(self.proc.pid), signal.SIGKILL)
        except Exception:
            try:
                self.proc.kill()
            except Exception:
                pass

    def teardown(self, graceful=True):
        if self.chan:
            self.chan.close()
            self.chan = None
        if self.proc and self.proc.poll() is None:
            if graceful and self.key:
                try:
                    ch = Channel(self.args.port, self.key)
                    ch.request("app.quit",
                               {"params": {"force": True}}, timeout=5)
                    ch.close()
                    self.proc.wait(timeout=8)
                except Exception:
                    pass
            if self.proc.poll() is None:
                self._kill_tree()
                try:
                    self.proc.wait(timeout=10)
                except Exception:
                    pass
        self.proc = None
        if self.log:
            try:
                self.log.close()
            except Exception:
                pass
            self.log = None


def build_payloads(workdir):
    """The hostile params.data battery.  Entries are decoded bytes;
    None means the request carries no "data" member at all."""
    nonutf8 = bytes([0x80, 0x81, 0xFE, 0xFF, 0x00, 0x66,
                     0xC3, 0x28, 0xED, 0xA0, 0x80,
                     0xF4, 0x90, 0x80, 0x80])
    real = os.path.join(workdir, "real-target.abw").encode()
    return [
        ("absent", None),
        ("empty", b""),
        ("tiny", b"x"),
        ("huge-400k", b"A" * 400_000),
        ("non-utf8+nul", nonutf8),
        ("fmt-string", b"%s%s%n%n%x%x%p"),
        ("traversal", b"../../../../../../etc/passwd"),
        ("abs-path", b"/etc/passwd"),
        ("uri", b"file:///etc/passwd"),
        ("big-num", b"999999999999999999999999999999"),
        ("neg-num", b"-1"),
        ("exp-num", b"1e999"),
        ("nan", b"NaN"),
        ("markup", b'<p/><section props="a:b"/><!--x-->'
                   b"<script>alert(1)</script>"),
        ("rtf", b"{\\rtf1\\ansi\\deff0{\\fonttbl}}"),
        ("newlines+nul", b"line1\nline2\r\n\ttab\x00end"),
        ("in-root-path",
         os.path.join(workdir, "hostile-target.abw").encode()),
        ("real-file", real),
        ("real-file-uri", b"file://" + real),
        ("astral+combining",
         "\U0001F600\U0001F389e\u0301\u0302".encode()),
        ("combining-x500", ("e" + "\u0301" * 500).encode()),
        ("spaces", b" " * 1024),
        ("ctrl-chars", bytes(range(1, 32))),
        ("json-inject", b'","command":"toggleBold","z":"'),
    ]


def exec_request(chan, cmd, wire):
    """Raw engine.execute line.  'command' precedes 'data': the
    server's flat _json_str picks the first "command" occurrence in
    the line, so data bytes must sit after it."""
    rid = chan.next_id
    chan.next_id += 1
    line = (b'{"id":%d,"method":"engine.execute","key":"%s",'
            b'"command":"%s"'
            % (rid, chan.key.encode(), cmd.encode()))
    if wire is not None:
        line += b',"data":"' + wire_escape(wire) + b'"'
    line += b"}"
    return line, rid


def protocol_fuzz(app, report):
    """Phase 1: hostile bytes at the wire/protocol level.  Each case
    runs on a throwaway connection; the app must survive all of it."""
    eprint("[phase1] protocol-level hostility")
    key = app.key
    port = app.args.port
    cases = []

    def unauth(s, rd):
        s.sendall(b'{"id":1,"method":"engine.execute",'
                  b'"command":"toggleBold"}\n')
        rd()
    cases.append(("unauth-request", unauth))

    def nonjson(s, rd):
        s.sendall(b'{"id":0,"method":"auth","key":"%s"}\n'
                  % key.encode())
        rd()
        s.sendall(b'not json at all\n')
        rd()
    cases.append(("non-json-line", nonjson))

    def binary(s, rd):
        s.sendall(b'{"id":0,"method":"auth","key":"%s"}\n'
                  % key.encode())
        rd()
        s.sendall(bytes(range(200)) + b"\n")
        rd()
    cases.append(("binary-garbage", binary))

    def toolong(s, rd):
        s.sendall(b'{"id":0,"method":"auth","key":"%s"}\n'
                  % key.encode())
        rd()
        s.sendall(b'{"id":1,"method":"ping","key":"%s","pad":"'
                  % key.encode() + b"A" * 1_100_000 + b'"}\n')
        rd()
    cases.append(("oversize-line", toolong))

    def badnames(s, rd):
        s.sendall(b'{"id":0,"method":"auth","key":"%s"}\n'
                  % key.encode())
        rd()
        for r in [
            b'{"id":1,"method":"engine.execute","key":"%s",'
            b'"command":"no-such-cmd","data":"x"}' % key.encode(),
            b'{"id":2,"method":"engine.execute","key":"%s",'
            b'"command":"%s"}' % (key.encode(), b"B" * 200_000),
            b'{"id":3,"method":"bogus.method","key":"%s"}'
            % key.encode(),
            b'{"id":4,"method":"engine.execute"}',
        ]:
            s.sendall(r + b"\n")
            rd()
    cases.append(("hostile-names", badnames))

    fails = 0
    for name, fn in cases:
        if not app.alive():
            report["crashes"].append(("protocol", name,
                                      "app dead before case"))
            fails += 1
            break
        try:
            s = socket.create_connection(("127.0.0.1", port), 10)
            s.settimeout(15)
            f = s.makefile("rb")

            def rd(f=f):
                return f.readline()

            fn(s, rd)
            try:
                f.close()
            except Exception:
                pass
            s.close()
        except OSError as ex:
            eprint(f"  {name}: connection error ({ex})")
        if not app.alive():
            d = app.death()
            report["crashes"].append(("protocol", name, repr(d)))
            eprint(f"  {name}: CRASH {d}")
            fails += 1
            break
        eprint(f"  {name}: app survived")
    # the primary connection must still serve requests
    try:
        st, obj = app.chan.request("ping", {}, timeout=5)
        if st != "reply":
            raise OSError("no reply")
    except Exception:
        if not app.reconnect():
            report["crashes"].append(("protocol", "ping-after",
                                      "channel wedged"))
            fails += 1
    return fails


def run_set(app, cmds, payloads, args, report, progress, done_set,
            prep="clean"):
    """Fire every payload at every command in cmds.

    prep: 'clean' saves the doc before each command so close/open
    paths don't stall on a dirty-doc prompt; 'dirty' re-inserts one
    char before every payload so lifecycle commands actually reach
    their save-confirm prompt (the Escape assist cancels it)."""
    since_restart = 0
    for cmd in cmds:
        if prep == "clean" and app.alive():
            try:
                app.chan.request(
                    "file.save",
                    {"params": {"path": os.path.join(
                        app.workdir, "current.abw")}}, timeout=15)
            except Exception:
                pass
        blocked_payloads = 0
        clean_exits = 0
        for pi, (pname, pdata) in enumerate(payloads):
            tag = f"{cmd}\t{pi}:{pname}"
            if tag in done_set:
                continue
            # a command that parks in a dialog/nested loop does so
            # without ever looking at params.data — after a few
            # blocked payloads the rest add coverage of nothing and
            # only cost timeouts, so skip them
            if blocked_payloads >= 4:
                progress.write(f"{tag}\tSKIP\tblocked-cmd\n")
                progress.flush()
                continue
            # a lifecycle command that already exited cleanly a few
            # times will keep doing so identically — each exit costs
            # a full relaunch, so don't pay for all 24
            if clean_exits >= 3:
                progress.write(f"{tag}\tSKIP\texit-cmd\n")
                progress.flush()
                continue
            if not app.alive():
                app.launch()
                since_restart = 0
            if prep == "dirty":
                try:
                    app.chan.request(
                        "engine.execute",
                        {"command": "insertData", "data": "x"},
                        timeout=5)
                except Exception:
                    pass
            line, rid = exec_request(app.chan, cmd, pdata)
            try:
                app.chan.send_line(line)
            except OSError:
                d = app.death()
                if d and d[0] == "signal":
                    report["crashes"].append((cmd, pname, repr(d)))
                    progress.write(f"{tag}\tCRASH\t{d}\n")
                    progress.flush()
                    app.launch()
                    continue
                if not app.reconnect():
                    report["hangs"].append((cmd, pname,
                                            "reconnect failed"))
                    progress.write(f"{tag}\tSENDFAIL\n")
                    progress.flush()
                    app.launch()
                    continue
                try:
                    app.chan.send_line(line)
                except OSError as ex:
                    report["hangs"].append((cmd, pname,
                                            f"resend:{ex}"))
                    progress.write(f"{tag}\tSENDFAIL\t{ex}\n")
                    progress.flush()
                    app.launch()
                    continue

            # poll in slices so an XTest Escape can fire while a
            # nested-loop menu or modal prompt holds the Fn — the
            # cancel lets it return and the real reply then lands
            deadline = time.monotonic() + args.timeout
            next_esc = time.monotonic() + ESCAPE_AFTER
            outcome = None
            obj = None
            while outcome is None:
                now = time.monotonic()
                if now >= deadline:
                    outcome = "timeout"
                    break
                try:
                    st, obj = app.chan.read_reply(
                        rid, min(next_esc, deadline))
                except OSError as ex:
                    outcome = f"connerr:{ex}"
                    break
                if st == "reply":
                    outcome = "reply"
                elif st == "eof":
                    outcome = "eof"
                else:
                    try:
                        app.chan.request(
                            "ui.key",
                            {"params": {"key": "Escape"}}, timeout=0.5)
                    except Exception:
                        pass
                    next_esc = time.monotonic() + 1.5

            d = app.death()
            if d and d[0] == "signal":
                report["crashes"].append((cmd, pname, repr(d)))
                progress.write(f"{tag}\tCRASH\t{d}\n")
                progress.flush()
                eprint(f"  CRASH {cmd} [{pname}] -> {d}")
                app.launch()
                since_restart = 0
                continue
            if outcome == "timeout":
                # A reply that never lands means the Fn is parked in
                # a nested loop (context menu / modal dialog).  That
                # is only a hang if the whole app is wedged — probe on
                # a fresh connection before condemning it.
                if app.reconnect():
                    blocked_payloads += 1
                    report["blocked"].append((cmd, pname))
                    progress.write(f"{tag}\tBLOCKED\n")
                    progress.flush()
                    eprint(f"  BLOCK {cmd} [{pname}] "
                           f"(dialog/nested loop)")
                    for _ in range(3):
                        try:
                            app.chan.request(
                                "ui.key",
                                {"params": {"key": "Escape"}},
                                timeout=0.5)
                        except Exception:
                            pass
                    since_restart += 1
                    continue
                report["hangs"].append((cmd, pname))
                progress.write(f"{tag}\tHANG\n")
                progress.flush()
                eprint(f"  HANG  {cmd} [{pname}]")
                app.launch()
                since_restart = 0
                continue
            if d and d[0] == "exit":
                clean_exits += 1
                report["clean-exits"].add(cmd)
                progress.write(f"{tag}\tEXIT\trc={d[1]}\n")
                progress.flush()
                app.launch()
                since_restart = 0
                continue
            if outcome != "reply" and app.alive():
                # e.g. server dropped the conn — rebuild it once
                if not app.reconnect():
                    report["hangs"].append((cmd, pname,
                                            f"{outcome}+no-reconnect"))
                    progress.write(f"{tag}\tHANG\t{outcome}\n")
                    progress.flush()
                    app.launch()
                    since_restart = 0
                    continue
            if outcome == "reply" and obj and "error" in obj:
                progress.write(
                    f"{tag}\terr\t"
                    f"{obj['error'].get('message','')[:60]}\n")
            else:
                progress.write(f"{tag}\t{outcome}\n")
            progress.flush()
            since_restart += 1
            if since_restart >= args.restart_every:
                app.launch()
                since_restart = 0


def main():
    ap = argparse.ArgumentParser(
        description="hostile-params fuzz over all edit methods")
    ap.add_argument("--bin", default=os.path.join(REPO, "src/abinova"))
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--doc",
                    default=os.path.join(REPO,
                                         "test/wp/cov07/rich.abw"))
    ap.add_argument("--timeout", type=float, default=TIMEOUT)
    ap.add_argument("--restart-every", type=int, default=400)
    ap.add_argument("--progress",
                    default=os.path.join(tempfile.gettempdir(),
                                         "hostile-progress.log"))
    ap.add_argument("--resume", action="store_true")
    ap.add_argument("--only", default="")
    ap.add_argument("--skip", default="")
    ap.add_argument("--report",
                    default=os.path.join(tempfile.gettempdir(),
                                         "hostile-report.json"))
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    if args.port == 0:
        s = socket.socket()
        s.bind(("127.0.0.1", 0))
        args.port = s.getsockname()[1]
        s.close()

    workdir = tempfile.mkdtemp(prefix="hostile-")
    docpath = os.path.join(workdir, "scratch.abw")
    shutil.copy(args.doc, docpath)
    # a real file under the roots for path-confined commands
    shutil.copy(args.doc, os.path.join(workdir, "real-target.abw"))

    done_set = set()
    prior_crashes = []
    prior_hangs = []
    if args.resume and os.path.exists(args.progress):
        with open(args.progress, errors="replace") as fh:
            for ln in fh:
                parts = ln.rstrip("\n").split("\t")
                if len(parts) < 2:
                    continue
                # line format is "cmd\tpi:pname\toutcome" — the resume
                # key must be the full cmd+payload tag, not just cmd
                done_set.add("\t".join(parts[:2]))
                cmd = parts[0]
                pname = parts[1].split(":", 1)[-1]
                res = parts[2] if len(parts) > 2 else ""
                if res == "CRASH":
                    prior_crashes.append((cmd, pname,
                                          parts[3] if len(parts) > 3
                                          else ""))
                elif res == "HANG":
                    prior_hangs.append((cmd, pname))

    progress = open(args.progress, "a", buffering=1)
    payloads = build_payloads(workdir)

    app = App(args, workdir, docpath)
    try:
        app.launch()
    except RuntimeError as ex:
        eprint(f"FATAL: {ex}")
        return 2

    st, obj = app.chan.request("engine.commands", {})
    if st != "reply" or "result" not in obj:
        app.teardown()
        eprint("FATAL: engine.commands failed:", obj)
        return 2
    names = [c["name"] for c in obj["result"]["commands"]]
    eprint(f"[setup] {len(names)} edit methods registered")
    if args.only:
        names = [n for n in names if re.search(args.only, n)]
    if args.skip:
        names = [n for n in names if not re.search(args.skip, n)]
    main_cmds = [n for n in names if not DEFER_RE.search(n)]
    tail_cmds = [n for n in names if DEFER_RE.search(n)]
    eprint(f"[setup] {len(main_cmds)} sweep + {len(tail_cmds)} "
           f"lifecycle-deferred")

    report = {"commands": len(names), "payloads": len(payloads),
              "crashes": prior_crashes, "hangs": prior_hangs,
              "blocked": [], "clean-exits": set()}

    try:
        protocol_fuzz(app, report)
        run_set(app, main_cmds, payloads, args, report, progress,
                done_set, prep="clean")
        run_set(app, tail_cmds, payloads, args, report, progress,
                done_set, prep="dirty")
    finally:
        app.teardown()

    report["clean-exits"] = sorted(report["clean-exits"])
    with open(args.report, "w") as fh:
        json.dump(report, fh, indent=1)

    eprint("=" * 60)
    eprint(f"sweep done: {len(names)} commands x "
           f"{len(payloads)} payloads")
    eprint(f"crashes: {len(report['crashes'])}")
    for c in report["crashes"]:
        eprint(f"   {c}")
    eprint(f"hangs:   {len(report['hangs'])}")
    for h in report["hangs"]:
        eprint(f"   {h}")
    eprint(f"blocked (dialog/nested loop, app stayed responsive): "
           f"{len(report['blocked'])}")
    for b in report["blocked"]:
        eprint(f"   {b}")
    eprint(f"clean-exit commands: {report['clean-exits']}")
    eprint(f"report: {args.report}")
    return 1 if (report["crashes"] or report["hangs"]) else 0


if __name__ == "__main__":
    sys.exit(main())
