# Agent notes

## Build

- `cd src && make -j2` — the in-tree build is configured
  `--enable-maintainer-mode --enable-coverage` **against the
  GTK 4.22 dev prefix** at `~/.local/abinova-gtk-dev` (system GTK
  is only 4.14; the configure floor is 4.22 since W30). The prefix
  lib dir is baked into the link as rpath, so `src/abinova` runs
  without env vars.
- If `configure.ac`/`Makefile.am` changes, the triggered
  `config.status --recheck` re-runs configure **without**
  `PKG_CONFIG_PATH` — source `~/.local/abinova-gtk-dev/env.sh`
  before `make` or it fails to find gtk4 >= 4.22.
- `tools/build-gtk-prefix.sh` rebuilds the prefix (sources live in
  `$GTKDEV_PREFIX/src`).
- Headless check: `xvfb-run -a ./src/abinova --to=pdf
  --to-name=/tmp/x.pdf FILE` then `pdftoppm -png -r 72 /tmp/x.pdf /tmp/x`.
