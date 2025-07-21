#!/bin/bash

set -euo pipefail

APP_NAME="vaulthalla"
echo "🔨 Building ${APP_NAME} package..."
VERSION=$(grep 'version' meson.build | sed -E 's/.*["'\'']([^"'\''"]+)["'\''].*/\1/')
echo "📦 Version detected: ${VERSION}"
ARCHITECTURE="$(dpkg --print-architecture)"
BUILD_DIR="build"
STAGING_DIR="deploy/${APP_NAME}_${VERSION}"
OUTPUT_DIR="dist"

echo "🔧 Building ${APP_NAME} version ${VERSION}..."

# Clean previous builds
rm -rf "${BUILD_DIR}" "${STAGING_DIR}"
meson setup "${BUILD_DIR}"
meson compile -C "${BUILD_DIR}"

# Install to a staging directory that mimics system layout
DESTDIR="${PWD}/${STAGING_DIR}" meson install -C "${BUILD_DIR}"

# Add control metadata
mkdir -p "${STAGING_DIR}/DEBIAN"

cat > "${STAGING_DIR}/DEBIAN/control" <<EOF
Package: ${APP_NAME}
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: ${ARCHITECTURE}
Maintainer: Cooper Larson <you@example.com>
Description: Vaulthalla self-hosted cloud platform
 Long description can go here.
EOF

# Optional postinst: enable systemd service, generate config, etc
# chmod +x "${STAGING_DIR}/DEBIAN/postinst"

# Build the .deb
mkdir -p "${OUTPUT_DIR}"
dpkg-deb --build "${STAGING_DIR}" "${OUTPUT_DIR}/${APP_NAME}_${VERSION}_${ARCHITECTURE}.deb"

echo "✅ Package created at ${OUTPUT_DIR}/${APP_NAME}_${VERSION}_${ARCHITECTURE}.deb"
