#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

: "${PREFIX:=/usr}"
: "${DATADIR:=$PREFIX/share}"

WEB_SOURCE_DIR="$ROOT_DIR/web"
WEB_STANDALONE_DIR="$WEB_SOURCE_DIR/.next/standalone"
WEB_STATIC_DIR="$WEB_SOURCE_DIR/.next/static"
WEB_PUBLIC_DIR="$WEB_SOURCE_DIR/public"
WEB_INSTALL_DIR="$DATADIR/vaulthalla-web"

log() {
  echo "[install_web] $*"
}

fail() {
  echo "[install_web] ERROR: $*" >&2
  exit 1
}

require_source_dir() {
  local dir="$1"
  local label="$2"
  if [[ ! -d "$dir" ]]; then
    fail "Missing $label at '$dir'. Run 'cd $WEB_SOURCE_DIR && pnpm build' before install."
  fi
}

log "Checking Next.js production artifacts..."
require_source_dir "$WEB_STANDALONE_DIR" "web standalone artifact directory"
require_source_dir "$WEB_STATIC_DIR" "web static artifact directory"

log "Installing web runtime payload to '$WEB_INSTALL_DIR'..."
sudo rm -rf "$WEB_INSTALL_DIR"
sudo install -d -m 0755 "$WEB_INSTALL_DIR"

# Keep Debian packaging layout semantics:
#   cp -a web/.next/standalone/. /usr/share/vaulthalla-web/
#   cp -a web/.next/static /usr/share/vaulthalla-web/.next/
sudo cp -a "$WEB_STANDALONE_DIR/." "$WEB_INSTALL_DIR/"
sudo install -d -m 0755 "$WEB_INSTALL_DIR/.next"
sudo cp -a "$WEB_STATIC_DIR" "$WEB_INSTALL_DIR/.next/"
if [[ -d "$WEB_PUBLIC_DIR" ]]; then
  sudo cp -a "$WEB_PUBLIC_DIR" "$WEB_INSTALL_DIR/"
fi

# Debian package files are root-owned at runtime.
sudo chown -R root:root "$WEB_INSTALL_DIR"

log "Validating installed web runtime payload..."
[[ -d "$WEB_INSTALL_DIR" ]] || fail "Expected web install directory missing: $WEB_INSTALL_DIR"
[[ -f "$WEB_INSTALL_DIR/server.js" ]] || fail "Missing web entrypoint: $WEB_INSTALL_DIR/server.js"
[[ -d "$WEB_INSTALL_DIR/.next/static" ]] || fail "Missing Next static directory: $WEB_INSTALL_DIR/.next/static"

log "Web artifact install complete."
