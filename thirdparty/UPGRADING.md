# Upgrading vendored libraries

`thirdparty/` bundles six libraries, each compiled as a libtool
`noinst` convenience archive (`-fPIC`) by `thirdparty/Makefile.am` and
folded statically into `libabinova`. There is no system dependency on
any of them.

`VENDORED.json` is the machine-readable inventory: for every library
it records the upstream URL, the exact tarball + sha256 this tree was
verified against, which upstream paths are vendored, and — critically —
which vendored files carry local patches vs. upstream. Verify it stays
honest with:

    tools/vendor-check.py            # all libs, downloads tarballs once
                                     # into .vendor-cache/
    tools/vendor-check.py wv         # one lib
    tools/vendor-check.py --tarball wv=/path/to/wv-1.2.9.tar.gz wv

## Per-library state (as of 2026-10-05)

| library          | vendored | upstream delta |
|------------------|----------|----------------|
| hunspell-1.7.4   | src/{hunspell,parsers,tools} + top docs/licenses | none — zero patched files; upstream build machinery, po/, man/, tests/ dropped |
| librevenge-0.0.6 | inc/ + src/lib/ + COPYING.*/README | none |
| libwpd-0.10.3    | inc/ + src/lib/ + COPYING.*/README | none |
| libwpg-0.3.4     | inc/ + src/lib/ + COPYING.*/README | none |
| libwps-0.4.14    | inc/ + src/lib/ + COPYING.*/README | none |
| wv-1.2.9         | flat library sources only          | **53 patched files** (~5.4k diff lines) + 3 local files — see below |

The five patch-free libraries are drop-in upgradeable. `wv` is not —
upstream died around 2010 and the vendored copy carries the entire
DOC01–23 security/correctness hardening series.

## Recipe: upgrading a patch-free library

(hunspell, librevenge, libwpd, libwpg, libwps)

1. Download the new upstream release tarball. Record it:
   `VENDORED.json` `tarball.url` + `tarball.sha256`, and bump `version`
   and `dir` (keep the `name-VERSION` directory convention).

2. Extract and keep only the paths listed in `vendored_paths`
   (inc/ + src/lib/ + licenses for the librevenge family; the file/dir
   list for hunspell). Everything else — upstream autotools, tests,
   converters, fuzzers — is deliberately not vendored; our
   `thirdparty/Makefile.am` is the only build system. Also delete any
   `Makefile.am`/`Makefile.in` inside the kept dirs (inc/, src/lib/):
   upstream's per-dir automake fragments are not vendored either.

3. Refresh `thirdparty/Makefile.am`:
   - rename the versioned dir in every `_SOURCES`/`-I` path
     (`libfoo-X.Y/src/lib/...`)
   - diff the new `src/lib/` (or `src/hunspell/`) file list against the
     old one: new upstream sources must be added to `_SOURCES`,
     removed ones dropped. `vendor-check.py` + a build will both flag
     mismatches.

4. Update version references: `thirdparty/README`, `README.md`,
   `configure.ac` checks, help/changelog where mentioned.

5. Rebuild and regression-test:

       make -C thirdparty && make -C src -j2
       make check          # unit suite incl. importer fixtures
       # spot converts: test/wp/*.doc, a .wpd, a docx, spell path

6. Run `tools/vendor-check.py <lib>` — the new tree should still show
   0 patched files. If the upgrade requires us to carry a fix, add the
   file to `patched_files` with a reason.

## Recipe: wv (the patched library)

wv-1.2.9 is upstream's last release (2010). The vendored tree =
upstream flat library sources + 53 patched files + `config.h`
(hand-written, defines `MATCHED_TYPE` — required), `version.c`
(generated), `wvblip.c` (local debug tool). Do NOT drop a new upstream
tree in blindly — the patches are the security posture.

If upstream ever revives, or the patch set must be rebased:

1. Regenerate the patch series:

       tools/vendor-check.py --patch-out patches/ wv
       # -> patches/wv/<file>.patch, one per patched file

2. Extract the new upstream tree, apply the series (`patch -p1`),
   review rejects, re-list files in `thirdparty/Makefile.am`
   `libwv_la_SOURCES`.

3. `patched_files` in `VENDORED.json` documents why each file differs —
   the reasons reference the DOC/UB/SEC audit tasks in `.devin/TASKS.md`
   and the git history that produced them.

Alternatively, a future task may replace wv entirely with a maintained
.doc parser; until then the vendored copy is effectively
maintained in-tree.

## After any upgrade

- `tools/vendor-check.py` must report OK for the lib (manifest ==
  reality).
- `make -C thirdparty && make -C src -j2` clean.
- `make check` green; convert a small corpus to pdf (`src/abinova
  --to=pdf --to-name=/tmp/x.pdf <file>`) over the affected formats.
- Bump `advisories` in `VENDORED.json` if the upgrade was driven by a
  CVE (VEND02 owns that field).
