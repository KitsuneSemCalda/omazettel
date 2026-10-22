#!/usr/bin/env bash
# OmaZettel installer, for Omarchy and Arch Linux.
#
#   curl -fsSL https://raw.githubusercontent.com/mcsmith3/omazettel/master/install.sh | bash
#
# Installs the pacman package from the latest release, after checking it
# against the release's SHA256SUMS. Where there is no package for this machine
# (such as aarch64), it builds one from the release's source instead. Run it
# again to update; remove it with: sudo pacman -R omazettel
#
# Environment:
#   OMAZETTEL_REF          the release tag to install, such as omazettel-v0.8.0 (default: the latest)
#   OMAZETTEL_FROM_SOURCE  set to 1 to build from source even where there is a package

set -euo pipefail

REPO="mcsmith3/omazettel"
REF="${OMAZETTEL_REF:-}"
FROM_SOURCE="${OMAZETTEL_FROM_SOURCE:-}"

info() { printf '  %s\n' "$*"; }
die() { printf '\e[31mError:\e[0m %s\n' "$*" >&2; exit 1; }

command -v pacman >/dev/null \
  || die "this installs a pacman package, and there is no pacman here (OmaZettel is for Omarchy and Arch Linux).
  To build it by hand, see https://github.com/$REPO#building-from-source"
command -v curl >/dev/null || die "curl is needed"
[[ $EUID -ne 0 ]] || die "run this as your own user, not root; it asks for sudo when it installs"

# Piped from curl, this script is on standard input: pacman and makepkg ask
# their questions on the terminal.
TTY=/dev/null
CONFIRM=(--noconfirm)
if [[ -r /dev/tty ]] && { : </dev/tty; } 2>/dev/null; then
  TTY=/dev/tty
  CONFIRM=()
fi
STALE="pacman did not install it. If it could not satisfy a dependency on qt6, update the system (sudo pacman -Syu) and run this again"

ARCH="$(uname -m)"
PACKAGE="omazettel-$ARCH.pkg.tar.zst"
if [[ -n $REF ]]; then
  BASE="https://github.com/$REPO/releases/download/$REF"
else
  BASE="https://github.com/$REPO/releases/latest/download"
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

build_from_source() {
  local tag="$REF"
  if [[ -z $tag ]]; then
    # The latest release's tag, from where /releases/latest redirects to.
    tag="$(curl -fsSLI -o /dev/null -w '%{url_effective}' "https://github.com/$REPO/releases/latest")"
    tag="${tag##*/}"
    [[ $tag == omazettel-v* ]] || die "could not find the latest release of $REPO"
  fi
  info "Building $tag from source (this takes a few minutes)"
  command -v git >/dev/null || die "building needs git: sudo pacman -S git base-devel"
  command -v makepkg >/dev/null || die "building needs makepkg: sudo pacman -S base-devel"
  git -c advice.detachedHead=false clone --quiet --depth 1 --branch "$tag" "https://github.com/$REPO.git" "$TMP/omazettel" \
    || die "could not download the source of $tag"
  # makepkg -s installs Qt and the compiler first if they are missing.
  (cd "$TMP/omazettel/pkgbuild" && makepkg -sfi "${CONFIRM[@]}" <"$TTY") || die "$STALE"
}

printf '\e[1m%s\e[0m\n' "Installing OmaZettel (${REF:-latest release}, $ARCH)"

if [[ -n $FROM_SOURCE ]]; then
  build_from_source
elif curl -fsSL -o "$TMP/$PACKAGE" "$BASE/$PACKAGE" 2>/dev/null; then
  info "Fetched $PACKAGE"
  curl -fsSL -o "$TMP/SHA256SUMS" "$BASE/SHA256SUMS" \
    || die "could not fetch $BASE/SHA256SUMS; nothing was installed"
  info "Checking it"
  (cd "$TMP" && grep " $PACKAGE\$" SHA256SUMS | sha256sum --check --quiet --strict) \
    || die "$PACKAGE is not what SHA256SUMS says it should be; nothing was installed"
  info "Installing it with pacman"
  sudo pacman -U "${CONFIRM[@]}" "$TMP/$PACKAGE" <"$TTY" || die "$STALE"
else
  info "There is no ready-made package for $ARCH in this release"
  build_from_source
fi

info "Done: $(pacman -Q omazettel)"
info "Open it from the launcher, or run: omazettel"
info "Update: run the same line again. Uninstall: sudo pacman -R omazettel (your notes stay)"
