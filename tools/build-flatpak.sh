#!/bin/sh
# build-flatpak.sh — build the Abinova Flatpak with flatpak-builder.
#
# Drives io.github.janos_szenfner.Abinova.yml (repo root): resolves the
# org.gnome Platform/Sdk from flathub, builds the manifest into a local
# build dir + repo, smoke-tests `abinova --version` inside the build
# sandbox, and optionally installs user-level or exports a .flatpak
# single-file bundle.
#
# Usage:
#   tools/build-flatpak.sh [--install] [--bundle [FILE]]
#                          [--builddir DIR] [--repo DIR] [--jobs N]
#                          [--no-smoke]
#
# Requires flatpak-builder (Debian: apt install flatpak-builder;
# openSUSE: zypper install flatpak-builder) and a flathub remote, which
# the script adds if absent.  The manifest pins org.gnome.Platform//51;
# any 49+ satisfies the tree's glib/gtk floors.
#
# The abinova module's `dir` source copies the whole tree — build from a
# clean checkout so stray build dirs (build-*/, fuzz-build/, san-build/,
# dist/ staging) do not get dragged into the build sandbox.

set -e

manifest=io.github.janos_szenfner.Abinova.yml
appid=io.github.janos_szenfner.Abinova

install=0
bundle=0
bundle_file=""
smoke=1
jobs=$(nproc 2>/dev/null || echo 4)

while [ $# -gt 0 ]; do
	case "$1" in
	--install)      install=1; shift ;;
	--bundle)       bundle=1; shift ;;
	--bundle=*)     bundle=1; bundle_file=${1#*=}; shift ;;
	--builddir)     builddir=$2; shift 2 ;;
	--builddir=*)   builddir=${1#*=}; shift ;;
	--repo)         repo=$2; shift 2 ;;
	--repo=*)       repo=${1#*=}; shift ;;
	--jobs|-j)      jobs=$2; shift 2 ;;
	--jobs=*|-j=*)  jobs=${1#*=}; shift ;;
	--no-smoke)     smoke=0; shift ;;
	-h|--help)
		sed -n '2,23p' "$0"; exit 0 ;;
	*) echo "build-flatpak: unknown option $1" >&2; exit 1 ;;
	esac
done

top=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
manifest_path="$top/$manifest"
builddir=${builddir:-"$top/build-flatpak"}
repo=${repo:-"$top/build-flatpak-repo"}

if [ ! -f "$manifest_path" ]; then
	echo "build-flatpak: manifest not found at $manifest_path" >&2
	exit 1
fi

if ! command -v flatpak-builder >/dev/null 2>&1; then
	cat >&2 <<'EOF'
build-flatpak: flatpak-builder not found — install it first:
  Debian/Ubuntu:  apt install flatpak-builder
  openSUSE:       zypper install flatpak-builder
  Fedora:         dnf install flatpak-builder
  or via flatpak: flatpak install org.flatpak.Builder
EOF
	exit 1
fi

# ------------------------------------------------- flathub remote
# Needed to fetch org.gnome.Platform//Sdk — add user-level if absent.
if ! flatpak remotes --columns=name 2>/dev/null | grep -qx flathub; then
	flatpak remote-add --if-not-exists --user flathub \
		https://flathub.org/repo/flathub.flatpakrepo
fi

# --------------------------------------------------------- build
fb_args="--force-clean --user --install-deps-from=flathub"
fb_args="$fb_args --repo=$repo"
# flatpak-builder takes its own -j semantics via default jobs arg
[ -n "$jobs" ] && fb_args="$fb_args --jobs=$jobs"
[ "$install" -eq 1 ] && fb_args="$fb_args --install"

# shellcheck disable=SC2086
flatpak-builder $fb_args "$builddir" "$manifest_path"

# ------------------------------------------------------ smoke test
# Runs inside the build sandbox — no install needed.
if [ "$smoke" -eq 1 ]; then
	echo "build-flatpak: smoke test — abinova --version"
	flatpak-builder --run "$builddir" "$manifest_path" abinova --version
fi

# ---------------------------------------------------------- bundle
if [ "$bundle" -eq 1 ]; then
	arch=$(flatpak --default-arch 2>/dev/null || uname -m)
	if [ -z "$bundle_file" ]; then
		# abi_version is an m4 composite of the three parts
		ver=$(sed -n 's/^m4_define(\[abi_version_\(major\|minor\|micro\)\], \[\([0-9]*\)\].*/\2/p' \
			"$top/configure.ac" | paste -sd. -)
		bundle_file="$top/dist/abinova-${ver:-0.0.0}-$arch.flatpak"
	fi
	mkdir -p "$(dirname "$bundle_file")"
	flatpak build-bundle \
		--runtime-repo=https://flathub.org/repo/flathub.flatpakrepo \
		"$repo" "$bundle_file" "$appid"
	echo "build-flatpak: wrote $bundle_file"
fi

cat <<EOF

Flatpak build complete.
  build dir: $builddir
  repo:      $repo
Run it without installing:
  flatpak-builder --run $builddir $manifest abinova
Install user-level:
  tools/build-flatpak.sh --install   # then: flatpak run $appid
EOF
