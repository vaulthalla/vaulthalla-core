#!/bin/bash

set -euo pipefail

echo "🔍 Checking for required build dependencies..."

echo "🔗 Installing Vaulthalla public key..."
sudo curl -fsSL https://apt.vaulthalla.sh/pubkey.gpg | sudo gpg --dearmor --y -o /etc/apt/trusted.gpg.d/vaulthalla.gpg

# Check if Vaulthalla debian source list is available
if ! grep -q "https://apt.vaulthalla.sh" /etc/apt/sources.list.d/vaulthalla.list; then
    echo "🔗 Adding Vaulthalla repository..."
    echo "deb [arch=amd64] https://apt.vaulthalla.sh stable main" | sudo tee /etc/apt/sources.list.d/vaulthalla.list > /dev/null
else
    echo "✅ Vaulthalla repository already exists."
fi

sudo apt update

check_pkg() {
    local pkg="$1"
    local desc="$2"
    if ! dpkg-query -W -f='${Status}' "$pkg" 2>/dev/null | grep -q "ok installed"; then
        echo "🔌 Installing $desc..."
        sudo apt install -y "$pkg"
    else
        echo "✅ $desc already installed."
    fi
}

# -- Vaulthalla maintained packages --
check_pkg libpdfium-dev "libpdfium-dev"
check_pkg libpqxx-dev "libpqxx-dev"

# -- Build tools --
if ! command -v meson &>/dev/null || ! command -v ninja &>/dev/null; then
    echo "🛠️ Installing Meson and Ninja build system..."
    sudo apt install -y meson ninja-build
else
    echo "✅ Meson and Ninja already installed."
fi

# -- Libraries --
check_pkg pkg-config "pkg-config"
check_pkg libturbojpeg0-dev "libturbojpeg0-dev"
check_pkg libmagic1 "libmagic1"
check_pkg libmagic-dev "libmagic-dev"
check_pkg libsodium-dev "libsodium-dev"
check_pkg libcurl4-openssl-dev "libcurl4-openssl-dev"
check_pkg uuid-dev "uuid-dev"
check_pkg libfuse3-dev "libfuse3-dev"
check_pkg libyaml-cpp-dev "libyaml-cpp-dev"
check_pkg libpugixml-dev "libpugixml-dev"
check_pkg libgtest-dev "libgtest-dev"
check_pkg libboost-filesystem-dev "Boost filesystem"
check_pkg libboost-system-dev "Boost system"
check_pkg libspdlog-dev "libspdlog-dev"
check_pkg libtss2-dev "libtss2-dev"
check_pkg tpm2-tools "tpm2-tools"
