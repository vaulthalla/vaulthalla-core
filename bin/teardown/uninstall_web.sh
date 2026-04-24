#!/usr/bin/env bash
set -euo pipefail

: "${PREFIX:=/usr}"
: "${DATADIR:=$PREFIX/share}"

WEB_INSTALL_DIR="$DATADIR/vaulthalla-web"

echo "[uninstall_web] Removing web runtime payload at '$WEB_INSTALL_DIR'..."
sudo rm -rf "$WEB_INSTALL_DIR"
echo "[uninstall_web] Web runtime payload removed."
