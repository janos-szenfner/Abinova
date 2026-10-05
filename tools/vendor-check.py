#!/usr/bin/env python3
"""Verify the vendored thirdparty/ trees against upstream release tarballs.

Reads thirdparty/VENDORED.json, extracts each library's upstream
tarball (downloaded on demand into --cache-dir), and classifies every
git-tracked vendored file as:

  unmodified   byte-identical to upstream
  patched      differs from upstream  -> must be listed in
               manifest patched_files
  local        absent upstream        -> must be listed in
               manifest local_files

Files only in the upstream tarball are dropped-vendored content and are
reported as a summary.  Exits non-zero if a patched/local file is not
classified in the manifest or a manifest entry names a file that no
longer exists -- i.e. the manifest must match reality.

Usage:
  tools/vendor-check.py [lib ...]           verify (all libs if none)
  tools/vendor-check.py --patch-out DIR lib write per-file diffs of the
                          patched files (the local patch series a
                          future upgrade must re-apply)
  tools/vendor-check.py --tarball lib=path  use a local tarball instead
                          of downloading
"""

import json
import os
import subprocess
import sys
import tarfile
import tempfile
import urllib.request

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MANIFEST = os.path.join(REPO, "thirdparty", "VENDORED.json")
DEFAULT_CACHE = os.path.join(REPO, ".vendor-cache")


def tracked_files(dirpath):
    """git-tracked files under dirpath, as repo-relative paths."""
    out = subprocess.run(
        ["git", "ls-files", dirpath], cwd=REPO, capture_output=True,
        text=True, check=True)
    return [l for l in out.stdout.splitlines() if l]


def fetch(url, dest):
    if os.path.exists(dest):
        return dest
    tmp = dest + ".part"
    print("  fetching %s" % url)
    urllib.request.urlretrieve(url, tmp)
    os.rename(tmp, dest)
    return dest


def extract(tarball, dest):
    with tarfile.open(tarball) as t:
        t.extractall(dest, filter="data")
    roots = [d for d in os.listdir(dest)
             if os.path.isdir(os.path.join(dest, d))]
    if len(roots) != 1:
        raise SystemExit("%s: expected one top dir, got %s" %
                         (tarball, roots))
    return os.path.join(dest, roots[0])


def walk(root):
    for dp, dns, fns in os.walk(root):
        dns[:] = sorted(dns)
        for f in sorted(fns):
            p = os.path.join(dp, f)
            yield os.path.relpath(p, root)


def check_lib(name, spec, tarball_map, cache_dir, patch_out):
    vdir = os.path.join(REPO, "thirdparty", spec["dir"])
    if not os.path.isdir(vdir):
        print("%-12s FAIL  vendored dir missing" % name)
        return False

    tarball = tarball_map.get(name)
    if not tarball:
        os.makedirs(cache_dir, exist_ok=True)
        tarball = fetch(spec["tarball"]["url"],
                        os.path.join(cache_dir,
                                     os.path.basename(
                                         spec["tarball"]["url"])))

    errors = []
    with tempfile.TemporaryDirectory() as tmp:
        udir = extract(tarball, tmp)

        vfiles = tracked_files(os.path.join("thirdparty", spec["dir"]))
        n_unmod = 0
        patched, local = [], []
        for rel_repo in vfiles:
            rel = os.path.relpath(rel_repo,
                                  os.path.join("thirdparty", spec["dir"]))
            upath = os.path.join(udir, rel)
            vpath = os.path.join(REPO, rel_repo)
            if not os.path.exists(upath):
                local.append(rel)
                continue
            with open(upath, "rb") as a, open(vpath, "rb") as b:
                if a.read() == b.read():
                    n_unmod += 1
                else:
                    patched.append(rel)

        ufiles = set(walk(udir))
        vset = set(os.path.relpath(
            f, os.path.join("thirdparty", spec["dir"])) for f in vfiles)
        dropped = sorted(ufiles - vset)

    man_patched = set(spec.get("patched_files", {}))
    man_local = set(spec.get("local_files", {}))
    for f in patched:
        if f not in man_patched:
            errors.append("unclassified patched file %s" % f)
    for f in man_patched:
        if f not in patched:
            errors.append("manifest patched_files entry %s does not "
                          "differ from upstream" % f)
    for f in local:
        if f not in man_local:
            errors.append("unclassified local file %s" % f)
    for f in man_local:
        if f not in local:
            errors.append("manifest local_files entry %s not found" % f)

    if errors:
        print("%-12s FAIL" % name)
        for e in errors:
            print("    %s" % e)
        return False

    print("%-12s OK    %d unmodified, %d patched, %d local, "
          "%d upstream-only (dropped)" %
          (name, n_unmod, len(patched), len(local), len(dropped)))

    if patch_out:
        os.makedirs(patch_out, exist_ok=True)
        with tempfile.TemporaryDirectory() as tmp:
            udir = extract(tarball, tmp)
            for rel in patched:
                d = subprocess.run(
                    ["diff", "-u",
                     "--label", "a/%s" % rel,
                     "--label", "b/%s" % rel,
                     os.path.join(udir, rel),
                     os.path.join(vdir, rel)],
                    capture_output=True)
                out = os.path.join(patch_out, name, rel + ".patch")
                os.makedirs(os.path.dirname(out), exist_ok=True)
                with open(out, "wb") as f:
                    f.write(d.stdout)
        print("    patch series written to %s/%s/" % (patch_out, name))
    return True


def main(argv):
    with open(MANIFEST) as f:
        manifest = json.load(f)
    libs = manifest["libraries"]

    tarball_map = {}
    patch_out = None
    want = []
    i = 0
    cache_dir = DEFAULT_CACHE
    while i < len(argv):
        a = argv[i]
        if a == "--tarball":
            i += 1
            k, _, v = argv[i].partition("=")
            tarball_map[k] = v
        elif a == "--patch-out":
            i += 1
            patch_out = argv[i]
        elif a == "--cache-dir":
            i += 1
            cache_dir = argv[i]
        elif a in libs:
            want.append(a)
        else:
            raise SystemExit("unknown arg: %s" % a)
        i += 1
    if not want:
        want = list(libs)

    ok = True
    for name in want:
        ok = check_lib(name, libs[name], tarball_map, cache_dir,
                       patch_out) and ok
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
