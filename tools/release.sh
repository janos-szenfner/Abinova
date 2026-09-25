#!/bin/bash
# Cut an Abinova release: tag, archive, build the dist tarball,
# checksum and (optionally) GPG-sign it.
#
# The version is read from configure.ac, so there is nothing to
# edit here when the version changes.
#
#   ./release.sh            tag + build + verify (make distcheck)
#   SKIP_DISTCHECK=1 ./release.sh   tag + build tarball only
#   NO_GPG=1 ./release.sh   skip all signing (unsigned tag/tarball)
#
# Environment overrides: BRANCH (default: current branch),
# RELEASE_BASE_DIR (default: ~/tmp).

set -e

BRANCH="${BRANCH:-$(git rev-parse --abbrev-ref HEAD)}"
RELEASE="$(awk -F'[][]' '
    /m4_define\(\[abi_version_major\]/ {maj=$4}
    /m4_define\(\[abi_version_minor\]/ {min=$4}
    /m4_define\(\[abi_version_micro\]/ {mic=$4}
    END {printf "%s.%s.%s", maj, min, mic}' configure.ac)"
RELEASE_BASE_DIR="${RELEASE_BASE_DIR:-$HOME/tmp}"
RELEASE_DIR="$RELEASE_BASE_DIR/abinova-release-dir-$RELEASE"
TARBALL_RE="abinova-*$RELEASE.tar.*"

TAG="release-$RELEASE"

if [ -z "$RELEASE" ] ; then
    echo "Could not read abi_version from configure.ac"
    exit 1
fi

# check for a git checkout
if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1 ; then
    echo "Must be in a git checkout"
    exit 1
fi

if ! git diff --quiet || ! git diff --cached --quiet ; then
    echo "Working tree is not clean. Commit first."
    exit 1
fi

if [ -n "$(git tag -l "$TAG")" ] ; then
    echo "Already found a tag for $RELEASE"
    exit 1
fi

if [ -d "$RELEASE_DIR" ] ; then
    echo "Unclean release. Directory $RELEASE_DIR exists."
    exit 2
fi

if [ ! -d "$RELEASE_BASE_DIR" ] ; then
    echo "I am about to create $RELEASE_BASE_DIR."
    echo "Continue [y/n]?"
    read -r c
    if [ "x$c" != "xy" ] ; then
        echo "Cancelled"
        exit 3
    fi
fi

mkdir -p "$RELEASE_DIR"

if [ -z "$NO_GPG" ] ; then
    echo "About to tag. Your GPG passphrase will be requested"
    git tag -s -m "release $RELEASE" "$TAG" "$BRANCH" || exit 4
else
    git tag -m "release $RELEASE" "$TAG" "$BRANCH" || exit 4
fi

mkdir "$RELEASE_DIR/abinova-$RELEASE"
git archive --format=tar "$TAG" | tar -x -C "$RELEASE_DIR/abinova-$RELEASE"

cd "$RELEASE_DIR/abinova-$RELEASE"
./autogen.sh
if [ -z "$SKIP_DISTCHECK" ] ; then
    make distcheck
else
    make dist
fi

cd "$RELEASE_DIR/abinova-$RELEASE"
sha256sum $TARBALL_RE > SHA256SUM
sha1sum $TARBALL_RE > SHA1SUM
if [ -z "$NO_GPG" ] ; then
    echo "Will sign the tarball. Your GPG passphrase will be requested"
    for t in $TARBALL_RE ; do gpg -ba "$t" ; done
fi

echo "Tarball(s) ready in $RELEASE_DIR/abinova-$RELEASE/:"
ls -1 $TARBALL_RE
echo "After everything is OK don't forget to git push --tags."
