#!/usr/bin/env python3
# Abinova — W14 recovery-hardening drive.
#
# Exercises the autosave/recovery path end-to-end over the control
# channel (real instances sharing one XDG_CONFIG_HOME, hence one
# autosave directory):
#
#   1. a dirty document's autosave produces a decode-valid recovery
#      copy plus its .info sidecar
#   2. an externally held instance lock defers the write — the last
#      good copy is never torn
#   3. a password-protected document leaves no plaintext recovery copy
#   4. two instances writing the same recovery name leave a file that
#      always decodes to one complete document
#   5. abandoned .part / orphaned .info temps are reclaimed at startup
#
# usage: tools/recovery-check.py [--bin src/abinova]
# exit: 0 pass, 1 a check failed, 2 infrastructure failure

import fcntl  # noqa: F401  (kept for parity with the lock holder)
import gzip
import json
import os
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET

BIN = os.path.join(os.path.dirname(__file__), "..", "src", "abinova")
CHECKS = []


def check(name, ok, detail=""):
    CHECKS.append((name, bool(ok), detail))
    print(("PASS" if ok else "FAIL"), name,
          ("— " + str(detail)) if detail else "")


def wait_for(pred, timeout=15.0, interval=0.25):
    """Poll pred() until true or timeout; returns the last value."""
    deadline = time.monotonic() + timeout
    val = pred()
    while not val and time.monotonic() < deadline:
        time.sleep(interval)
        val = pred()
    return val


class Channel:
    def __init__(self, port, key):
        self.sock = socket.create_connection(("127.0.0.1", port), 10)
        self.key = key
        self.next_id = 1
        self.buf = b""

    def request(self, method, extra=None, timeout=30):
        rid = self.next_id
        self.next_id += 1
        req = {"id": rid, "method": method, "key": self.key}
        req.update(extra or {})
        self.sock.sendall(json.dumps(req).encode() + b"\n")
        deadline = time.monotonic() + timeout
        while b"\n" not in self.buf:
            left = deadline - time.monotonic()
            if left <= 0:
                raise TimeoutError(method)
            self.sock.settimeout(left)
            chunk = self.sock.recv(1 << 20)
            if not chunk:
                raise ConnectionError("control channel closed")
            self.buf += chunk
        line, self.buf = self.buf.split(b"\n", 1)
        return json.loads(line)


class App:
    """One abinova instance under xvfb with the control channel up."""

    def __init__(self, args, workdir, port, doc=None):
        self.port = port
        self.proc = None
        self.chan = None
        self.keypath = os.path.join(
            workdir, "config", "abinova", f"control-key.local-{port}")
        env = dict(os.environ)
        env["GDK_BACKEND"] = "x11"
        env.pop("WAYLAND_DISPLAY", None)
        env["G_APPLICATION_NON_UNIQUE"] = "1"
        env["GTK_USE_PORTAL"] = "0"
        env["NO_AT_BRIDGE"] = "1"
        env["XDG_CONFIG_HOME"] = os.path.join(workdir, "config")
        env["XDG_DATA_HOME"] = os.path.join(workdir, "data")
        env["XDG_CACHE_HOME"] = os.path.join(workdir, "cache")
        cmd = []
        if shutil.which("xvfb-run"):
            cmd += ["xvfb-run", "-a"]
        # private bus per instance: two apps on one bus would fold
        # into the single-instance remoting path instead of running
        if shutil.which("dbus-run-session"):
            cmd += ["dbus-run-session", "--"]
        cmd += [args.bin, f"--control={port}",
                f"--automation-read-root={workdir}",
                f"--automation-write-root={workdir}"]
        if doc:
            cmd.append(doc)
        self.log = open(os.path.join(workdir, f"app-{port}.log"), "wb",
                        buffering=0)
        self.proc = subprocess.Popen(cmd, env=env, stdout=self.log,
                                     stderr=self.log,
                                     start_new_session=True)
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            if os.path.exists(self.keypath):
                break
            if self.proc.poll() is not None:
                raise RuntimeError(f"abinova exited early on :{port}")
            time.sleep(0.1)
        self.key = open(self.keypath).read().strip()
        self.chan = Channel(port, self.key)
        r = self.chan.request("ping")
        if r.get("result") != "pong":
            raise RuntimeError("control channel did not answer ping")

    def close(self):
        try:
            self.chan.request("app.quit", {"force": True}, timeout=5)
        except Exception:
            pass
        try:
            self.proc.wait(10)
        except Exception:
            os.killpg(self.proc.pid, signal.SIGKILL)
            self.proc.wait()
        self.log.close()


class LockHolder:
    """Hold flock() on a path from a separate process."""

    def __init__(self, path):
        self.path = path
        self.proc = None

    def __enter__(self):
        self.proc = subprocess.Popen(
            [sys.executable, "-c",
             "import fcntl,sys,time\n"
             "f=open(sys.argv[1],'a+b',buffering=0)\n"
             "fcntl.flock(f.fileno(),fcntl.LOCK_EX)\n"
             "sys.stdout.write('L');sys.stdout.flush()\n"
             "time.sleep(60)",
             self.path],
            stdout=subprocess.PIPE)
        assert self.proc.stdout.read(1) == b"L"
        return self

    def __exit__(self, *a):
        self.proc.terminate()
        self.proc.wait()


def decode_abwn(path):
    """Decoded XML bytes of an .abwn file (gzip-transparent), or None."""
    try:
        data = open(path, "rb").read()
    except OSError:
        return None
    if data[:2] == b"\x1f\x8b":
        try:
            data = gzip.decompress(data)
        except Exception:
            return None
    try:
        ET.fromstring(data)
    except Exception:
        return None
    return data


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=BIN)
    ap.add_argument("--port", type=int, default=9740)
    args = ap.parse_args()

    workdir = tempfile.mkdtemp(prefix="abinova-w14-")
    autosave_dir = os.path.join(workdir, "config", "abinova", "autosave")
    doc1 = os.path.join(workdir, "doc1.abwn")
    doc2 = os.path.join(workdir, "doc2.abwn")
    apps = []
    try:
        app = App(args, workdir, args.port)
        apps.append(app)

        # --- 1: dirty-doc autosave ----------------------------------
        app.chan.request("ui.text", {"text": "hello recovery world"})
        app.chan.request("file.save", {"path": doc1})
        # a successful save marks the doc clean; type again so the
        # next tick has something to recover
        app.chan.request("ui.text", {"text": " dirty work"})
        r = app.chan.request("document.autosave")
        res = r.get("result", {})
        path = res.get("path", "")
        # the write lands on the async worker just after the reply —
        # poll for it rather than racing the enqueue
        check("autosave writes recovery copy for dirty doc",
              res.get("exists") is True or
              wait_for(lambda: os.path.isfile(path)), path)
        xml = decode_abwn(path)
        check("recovery copy decode-validates (gzip-transparent XML)",
              xml is not None)
        check("recovery copy contains the typed text",
              xml is not None and b"dirty work" in xml)
        # .info is written by the completion callback on the next
        # main-context turn after the worker lands the payload
        check(".info sidecar written alongside",
              wait_for(lambda: os.path.isfile(path + ".info")))

        # --- 2: held instance lock defers the write ------------------
        os.unlink(path)
        with LockHolder(path + ".lock"):
            app.chan.request("ui.text", {"text": " second edit"})
            # outlast the 20s typing-pause defer so the tick reaches
            # backup() itself
            time.sleep(21)
            app.chan.request("document.autosave")
            check("held lock: recovery write deferred, no torn file",
                  not os.path.isfile(path))
        app.chan.request("document.autosave")
        check("lock released: recovery write lands",
              wait_for(lambda: os.path.isfile(path)))
        xml = decode_abwn(path)
        check("post-lock copy carries the second edit",
              xml is not None and b"second edit" in xml)

        # --- 3: password-protected doc -------------------------------
        app.chan.request("ui.text", {"text": " verysecret"})
        app.chan.request("file.save",
                         {"path": doc2, "password": "w14-test-pw"})
        magic = open(doc2, "rb").read(7)
        check("file.save password produces an encrypted document",
              magic == b"ABWNCRP", repr(magic))
        app.chan.request("ui.text", {"text": " more secret"})
        time.sleep(21)  # past the typing-pause defer
        r = app.chan.request("document.autosave")
        res = r.get("result", {})
        ppath = res.get("path", "")
        check("protected doc writes no recovery copy",
              not os.path.isfile(ppath), ppath)
        check("protected doc's stale plaintext copy is removed",
              not os.path.isfile(path))
        leaked = False
        for fn in os.listdir(autosave_dir):
            fpath = os.path.join(autosave_dir, fn)
            try:
                raw = open(fpath, "rb").read()
            except OSError:
                continue
            if raw[:2] == b"\x1f\x8b":
                try:
                    raw = gzip.decompress(raw)
                except Exception:
                    continue
            if b"secret" in raw:
                leaked = True
        check("autosave dir holds no plaintext of the protected doc",
              not leaked)
        app.close()
        apps.remove(app)

        # --- 4: two instances, same recovery name --------------------
        # both launch untitled -> both get the "Untitled-1" token ->
        # the same backup name in the shared autosave dir
        instA = App(args, workdir, args.port + 1)
        instB = App(args, workdir, args.port + 2)
        apps += [instA, instB]
        instA.chan.request("ui.text", {"text": "INSTANCE_A_ALPHA"})
        instB.chan.request("ui.text", {"text": "INSTANCE_B_BETA"})
        rA = instA.chan.request("document.autosave")
        rB = instB.chan.request("document.autosave")
        pathA = rA.get("result", {}).get("path", "")
        pathB = rB.get("result", {}).get("path", "")
        check("two instances resolve the same recovery name",
              pathA and pathA == pathB, f"{pathA} vs {pathB}")
        shared = pathA
        wait_for(lambda: os.path.isfile(shared))
        xml = decode_abwn(shared)
        a = xml is not None and b"INSTANCE_A_ALPHA" in xml
        b = xml is not None and b"INSTANCE_B_BETA" in xml
        check("shared write: file is one complete doc, not torn",
              xml is not None and a != b, f"A={a} B={b}")
        # second round after the typing-pause defer expires
        instA.chan.request("ui.text", {"text": " MORE_A"})
        instB.chan.request("ui.text", {"text": " MORE_B"})
        time.sleep(21)
        instA.chan.request("document.autosave")
        instB.chan.request("document.autosave")
        # whichever instance won the name lock this round — the file
        # must end up holding exactly one complete second-round doc
        def _round2_landed():
            x = decode_abwn(shared)
            return x is not None and (b"MORE_A" in x or b"MORE_B" in x)
        wait_for(_round2_landed)
        xml = decode_abwn(shared)
        a = xml is not None and b"INSTANCE_A_ALPHA" in xml
        b = xml is not None and b"INSTANCE_B_BETA" in xml
        check("second shared write: still one complete doc",
              xml is not None and a != b, f"A={a} B={b}")
        for inst in (instA, instB):
            inst.close()
            apps.remove(inst)

        # --- 5: abandoned temp cleanup at startup --------------------
        os.makedirs(autosave_dir, exist_ok=True)
        stale = os.path.join(autosave_dir, "stale.saved.part")
        orphan = os.path.join(autosave_dir, "orphan.saved.info")
        lockfile = os.path.join(autosave_dir, "doc.saved.lock")
        for p in (stale, orphan, lockfile):
            open(p, "w").write("x")
        old = time.time() - 3600
        for p in (stale, orphan, lockfile):
            os.utime(p, (old, old))
        app = App(args, workdir, args.port + 3)
        apps.append(app)
        # the control channel answers before the startup recovery
        # scan runs, so give cleanup a moment to land
        check("startup reclaims stale .part",
              wait_for(lambda: not os.path.exists(stale)))
        check("startup reclaims orphaned .info",
              wait_for(lambda: not os.path.exists(orphan)))
        time.sleep(1)
        check("startup keeps lock sidecar", os.path.exists(lockfile))
        app.close()
        apps.remove(app)
    finally:
        for a in apps:
            try:
                a.close()
            except Exception:
                pass

    failed = [c for c in CHECKS if not c[1]]
    print(f"\n{len(CHECKS)} checks, {len(failed)} failures")
    if not failed:
        shutil.rmtree(workdir, ignore_errors=True)
    else:
        print("workdir kept:", workdir)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
