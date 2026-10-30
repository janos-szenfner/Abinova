# Agent notes

## Build

- `cd src && make -j2` — the in-tree build is configured
  `--enable-maintainer-mode --enable-coverage` **against the
  GTK 4.22 dev prefix** at `~/.local/abinova-gtk-dev` (system GTK
  is only 4.14; the configure floor is 4.20 since W30). The prefix
  lib dir is baked into the link as rpath, so `src/abinova` runs
  without env vars.
- If `configure.ac`/`Makefile.am` changes, the triggered
  `config.status --recheck` re-runs configure **without**
  `PKG_CONFIG_PATH` — source `~/.local/abinova-gtk-dev/env.sh`
  before `make` or it fails to find gtk4 >= 4.20.
- `tools/build-gtk-prefix.sh` rebuilds the prefix (sources live in
  `$GTKDEV_PREFIX/src`).
- Headless check: `xvfb-run -a ./src/abinova --to=pdf
  --to-name=/tmp/x.pdf FILE` then `pdftoppm -png -r 72 /tmp/x.pdf /tmp/x`.
- GUI/control-channel drives under Xvfb: export
  `LD_LIBRARY_PATH=~/.local/abinova-gtk-dev/lib` first — the binary
  otherwise mixes prefix pango with *system* harfbuzz 8.3 (rpath is
  not transitive for pango's own deps) and aborts with
  `hb_object_is_valid` in hb_object_destroy on the first reflow.
  The widget-paintable `ui.screenshot`/`ui.render` methods and
  `ui.key` XTest path are crash-prone in this env — prefer
  `engine.execute` + `document.*` + external `xwd` captures.
