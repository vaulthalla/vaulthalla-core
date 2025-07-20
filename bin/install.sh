#!/usr/bin/env bash
set -euo pipefail

# ═══════════════════════════════════════════════════════
#             ⚔️  VAULTHALLA INSTALLATION ⚔️
#       This script sets up the entire environment
# ═══════════════════════════════════════════════════════

DEV_MODE=false

# parse command line arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        -d|--dev)
            DEV_MODE=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done


# Check if we're root OR have passwordless sudo
if [[ $EUID -ne 0 ]]; then
    if ! sudo -n true 2>/dev/null; then
        echo "❗️ This script must be run as root or with passwordless sudo."
        exit 1
    fi
fi

# Clean build environment
#if [[ -d build ]]; then
#    echo "🧹 Cleaning previous build artifacts..."
#    rm -rf build
#else
#    mkdir -p build
#fi

# === 1) Create system user ===
if ! id vaulthalla &>/dev/null; then
    echo "👤 Creating system user 'vaulthalla'..."
    sudo useradd -r -s /usr/sbin/nologin vaulthalla
else
    echo "👤 System user 'vaulthalla' already exists."
fi

# === 2) Ensure Build Dependencies ===
echo "🔍 Checking for required build dependencies..."

sudo apt update

# -- LibMagic --
if ! dpkg -l | grep -q libmagic1; then
    echo "🔌 Installing libmagic1..."
    sudo apt install -y libmagic1
else
    echo "✅ libmagic1 already installed."
fi

if ! dpkg -l | grep -q libmagic-dev; then
    echo "🔌 Installing libmagic-dev..."
    sudo apt install -y libmagic-dev
else
    echo "✅ libmagic-dev already installed."
fi

# -- PostgreSQL client + libpqxx --
if ! dpkg -l | grep -q libpqxx-dev; then
    echo "🔌 Installing libpqxx..."
    sudo apt install -y libpqxx-dev
else
    echo "✅ libpqxx-dev already installed."
fi

# -- libsodium --
if ! dpkg -l | grep -q libsodium-dev; then
    echo "🔌 Installing libsodium-dev..."
    sudo apt install -y libsodium-dev
else
    echo "✅ libsodium-dev already installed."
fi

# -- libcurl --
if ! dpkg -l | grep -q libcurl4-openssl-dev; then
    echo "🔌 Installing libcurl4-openssl-dev..."
    sudo apt install -y libcurl4-openssl-dev
else
    echo "✅ libcurl4-openssl-dev already installed."
fi

# -- uuid --
if ! dpkg -l | grep -q uuid-dev; then
    echo "🔌 Installing uuid-dev..."
    sudo apt install -y uuid-dev
else
    echo "✅ uuid-dev already installed."
fi

# -- FUSE3 --
if ! dpkg -l | grep -q libfuse3-dev; then
    echo "🔌 Installing libfuse3-dev..."
    sudo apt install -y libfuse3-dev
else
    echo "✅ libfuse3-dev already installed."
fi

# -- libpoppler --
if ! dpkg -l | grep -q libpoppler-cpp-dev; then
    echo "🔌 Installing libpoppler-cpp-dev..."
    sudo apt install -y libpoppler-dev libpoppler-cpp-dev
else
    echo "✅ libpoppler-cpp-dev already installed."
fi

# -- yaml-cpp --
if ! dpkg -l | grep -q libyaml-cpp-dev; then
    echo "🔌 Installing libyaml-cpp-dev..."
    sudo apt install -y libyaml-cpp-dev
else
    echo "✅ libyaml-cpp-dev already installed."
fi

# -- pugixml --
if ! dpkg -l | grep -q libpugixml-dev; then
    echo "🔌 Installing libpugixml-dev..."
    sudo apt install -y libpugixml-dev
else
    echo "✅ libpugixml-dev already installed."
fi

# -- gtest --
if ! dpkg -l | grep -q libgtest-dev; then
    echo "🔌 Installing libgtest-dev..."
    sudo apt install -y libgtest-dev
else
    echo "✅ libgtest-dev already installed."
fi

# -- Boost (filesystem + system) --
if ! dpkg -l | grep -q libboost-filesystem-dev; then
    echo "🔌 Installing Boost (filesystem + system)..."
    sudo apt install -y libboost-filesystem-dev libboost-system-dev
else
    echo "✅ Boost components already installed."
fi

# === Conan ===
if ! command -v conan &>/dev/null; then
    echo "🧱 Conan not found. Attempting installation..."

    if command -v pip &>/dev/null; then
        echo "➡️ Trying pip --user install..."
        if pip install --user conan; then
            export PATH="$PATH:$HOME/.local/bin"
        else
            echo "⚠️ pip --user failed. Trying sudo pip..."

            if sudo pip install conan; then
                echo "✅ Installed Conan globally via pip."
            else
                echo "❌ pip failed. Trying pipx fallback..."

                if ! command -v pipx &>/dev/null; then
                    echo "📦 Installing pipx..."
                    sudo apt install -y pipx
                fi

                echo "📦 Installing Conan globally via pipx..."
                sudo pipx install conan || true
                sudo pipx ensurepath

                # This is the pipx install path for root. Export early for next checks.
                ROOT_PIPX_BIN="/root/.local/bin"
                if [[ -x "$ROOT_PIPX_BIN/conan" ]]; then
                    echo "📍 Conan binary found in pipx fallback path. Injecting into PATH..."
                    export PATH="$PATH:$ROOT_PIPX_BIN"
                fi
            fi
        fi
    else
        echo "❌ pip is missing. Cannot install Conan."
        exit 1
    fi

    # Final sanity check (brute force path check fallback)
    if ! command -v conan &>/dev/null; then
        if [[ -x "/root/.local/bin/conan" ]]; then
            echo "⚠️ Conan manually found at /root/.local/bin/conan"
        else
            echo "💥 Conan install failed after all fallback attempts."
            exit 1
        fi
    fi

    echo "✅ Conan installed successfully."
else
    echo "✅ Conan already installed."
fi

# -- Meson/Ninja --
if ! command -v meson &>/dev/null || ! command -v ninja &>/dev/null; then
    echo "🛠️ Installing Meson and Ninja build system..."
    sudo apt install -y meson ninja-build
else
    echo "✅ Meson and Ninja already installed."
fi

# === 3) Build Project ===
echo "🏗️  Starting Vaulthalla build..."

echo "🔧 Installing Conan dependencies..."

# === Ensure default Conan profile exists ===
if [[ ! -f "/root/.conan2/profiles/default" ]]; then
    echo "📄 Conan profile missing. Running 'conan profile detect'..."
    conan profile detect --force
else
    echo "✅ Conan profile already exists."
fi

# === Conan Profile Detection & Injection ===
echo "🧠 Generating custom Vaulthalla Conan profile..."

# Get base profile dir (no matter pipx, sudo, root, user, etc)
CONAN_PROFILE_DIR=$(conan profile path default | sed 's|/default$||')
VAULTHALLA_PROFILE_PATH="$CONAN_PROFILE_DIR/vaulthalla"

# Detect compiler
if command -v g++ &> /dev/null; then
  COMPILER="gcc"
  COMPILER_VERSION=$(g++ -dumpversion | cut -d. -f1)
elif command -v clang++ &> /dev/null; then
  COMPILER="clang"
  COMPILER_VERSION=$(clang++ --version | grep -oP 'version\s+\K[0-9]+' | head -n1)
else
  echo "❌ No supported compiler found (gcc or clang required)."
  exit 1
fi

# Detect arch
ARCH=$(uname -m)
[[ "$ARCH" == "x86_64" ]] || {
  echo "❌ Unsupported architecture: $ARCH"
  exit 1
}

# Detect OS
OS=$(uname)
[[ "$OS" == "Linux" ]] || {
  echo "❌ Unsupported OS: $OS. Vaulthalla is Linux-only."
  exit 1
}

# Write profile
mkdir -p "$(dirname "$VAULTHALLA_PROFILE_PATH")"

BUILD_TYPE="Release"
if [ "$DEV_MODE" = true ]; then
  BUILD_TYPE="Debug"
fi

cat > "$VAULTHALLA_PROFILE_PATH" <<EOF
[settings]
arch=$ARCH
build_type=$BUILD_TYPE
compiler=$COMPILER
compiler.version=$COMPILER_VERSION
compiler.cppstd=gnu23
compiler.libcxx=libstdc++11
os=Linux

[conf]
tools.system.package_manager:mode = install
tools.system.package_manager:sudo = True
EOF

echo "✅ Custom Conan profile written to $VAULTHALLA_PROFILE_PATH"

# Install dependencies
conan install . --build=missing -pr:a vaulthalla

echo "🔨 Building binaries..."
conan build . -pr:a vaulthalla

# === 4) Setup Runtime Directories ===
echo "📁 Creating runtime directories..."
for dir in /mnt/vaulthalla /var/lib/vaulthalla /var/log/vaulthalla /run/vaulthalla; do
    sudo install -d -o vaulthalla -g vaulthalla -m 755 "$dir"
done
sudo chmod 750 /var/log/vaulthalla

# === 5) Install Binaries ===
echo "🚀 Installing core executables..."
sudo install -d -o vaulthalla -g vaulthalla -m 755 /usr/local/bin/vaulthalla
sudo install -m 755 build/fuse_daemon /usr/local/bin/vaulthalla/
sudo install -m 755 build/core_daemon /usr/local/bin/vaulthalla/

# === 6) Deploy Config ===
echo "⚙️  Deploying default config..."
sudo install -d -m 755 /etc/vaulthalla
sudo cp deploy/config/config_template.yaml.in /etc/vaulthalla/
if [[ -f ./config.yaml ]]; then
    echo "📄 Using local ./config.yaml"
    sudo cp ./config.yaml /etc/vaulthalla/config.yaml
else
    echo "📄 Using example config"
    sudo cp deploy/config/config.example.yaml /etc/vaulthalla/config.yaml
fi

# === 7) Setup Database ===
VAUL_PG_PASS=$(uuidgen)
echo "🔐 Creating PostgreSQL user and database..."

sudo -u postgres psql -tc "SELECT 1 FROM pg_roles WHERE rolname='vaulthalla'" | grep -q 1 ||
    sudo -u postgres psql -c "CREATE USER vaulthalla WITH PASSWORD '${VAUL_PG_PASS}';"

sudo -u postgres psql -tc "SELECT 1 FROM pg_database WHERE datname='vaulthalla'" | grep -q 1 ||
    sudo -u postgres psql -c "CREATE DATABASE vaulthalla OWNER vaulthalla;"

sudo -u postgres psql -c "GRANT ALL PRIVILEGES ON DATABASE vaulthalla TO vaulthalla;"

# === 8) Inject DB Password into Config ===
echo "✍️  Updating config with DB password..."
sudo sed -i "s/^\(\s*password:\s*\).*/\1${VAUL_PG_PASS}/" /etc/vaulthalla/config.yaml

# === 9) Apply Schema + Seed DB ===
for sql_file in auth vaults files sync acl; do
    echo "📄 Applying $sql_file.sql..."
    sudo -u vaulthalla psql -d vaulthalla -f "deploy/psql/$sql_file.sql"
done

echo "🌱 Seeding database..."
sudo -u vaulthalla psql -d vaulthalla -f deploy/psql/seed.sql

echo "🔐 Set admin password:"
ADMIN_PLAIN="${ADMIN_PLAIN:-vh!adm1n}"

if [[ "$DEV_MODE" == true ]]; then
    echo "⚠️  [DEV_MODE] Using default admin password vh!adm1n"
else
    while true; do
        echo "🔑 Please enter a secure admin password:"
        read -rs ADMIN_PLAIN
        echo

        if [[ -z "$ADMIN_PLAIN" ]]; then
            echo "❗ Password cannot be empty."
            continue
        fi

        # Try to hash and validate
        if HASHED_PASS=$(./build/hash_password --validate "$ADMIN_PLAIN" 2>&1); then
            echo "✅ Password accepted."
            break
        else
            echo "❌ Password rejected:"
            echo "$HASHED_PASS"
        fi
    done
fi

# In dev mode, we still hash after the conditional block:
if [[ "$DEV_MODE" == true ]]; then
    HASHED_PASS=$(./build/hash_password "$ADMIN_PLAIN")
fi

echo "🔑 Seeding admin user..."
sudo ./bin/setup/seed_admin.sh "$HASHED_PASS"

if [[ $DEV_MODE == true ]]; then
    echo "🔄 Running cloud test vault setup..."
    source ./bin/test/init_cloud_test_vault.sh
else
    echo "🌐 Skipping cloud test vault setup in production mode."
fi

# === 10) Install systemd services ===
echo "🛠️  Installing systemd services..."
sudo install -m 644 deploy/systemd/vaulthalla-core.service /etc/systemd/system/
sudo install -m 644 deploy/systemd/vaulthalla-fuse.service /etc/systemd/system/

sudo systemctl daemon-reload
sudo systemctl enable --now vaulthalla-core.service
sudo systemctl enable --now vaulthalla-fuse.service

echo ""
echo "🏁 Vaulthalla installed successfully!"

if [[ "$DEV_MODE" == true ]]; then
    sudo journalctl -f -u vaulthalla-core
fi
