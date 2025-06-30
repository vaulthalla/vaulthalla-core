#!/usr/bin/env bash
set -euo pipefail

# ═══════════════════════════════════════════════════════
#             ⚔️  VAULTHALLA INSTALLATION ⚔️
#       This script sets up the entire environment
# ═══════════════════════════════════════════════════════

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

# -- PostgreSQL --
if ! command -v psql &>/dev/null; then
    echo "🔌 Installing PostgreSQL client tools..."
    sudo apt update
    sudo apt install -y postgresql
else
    echo "✅ PostgreSQL already installed."
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

cat > "$VAULTHALLA_PROFILE_PATH" <<EOF
[settings]
arch=$ARCH
build_type=Release
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

# === 4) Deploy helper binary ===
echo "📁 Deploying hash_password helper..."
sudo mv build/hash_password deploy/psql/

# === 5) Setup Runtime Directories ===
echo "📁 Creating runtime directories..."
for dir in /mnt/vaulthalla /var/lib/vaulthalla /var/log/vaulthalla /run/vaulthalla; do
    sudo install -d -o vaulthalla -g vaulthalla -m 755 "$dir"
done
sudo chmod 750 /var/log/vaulthalla

# === 6) Install Binaries ===
echo "🚀 Installing core executables..."
sudo install -d -o vaulthalla -g vaulthalla -m 755 /usr/local/bin/vaulthalla
sudo install -m 755 build/fuse_daemon /usr/local/bin/vaulthalla/
sudo install -m 755 build/core_daemon /usr/local/bin/vaulthalla/

# === 7) Deploy Config ===
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

# === 8) Setup Database ===
VAUL_PG_PASS=$(uuidgen)
echo "🔐 Creating PostgreSQL user and database..."

sudo -u postgres psql -tc "SELECT 1 FROM pg_roles WHERE rolname='vaulthalla'" | grep -q 1 ||
    sudo -u postgres psql -c "CREATE USER vaulthalla WITH PASSWORD '${VAUL_PG_PASS}';"

sudo -u postgres psql -tc "SELECT 1 FROM pg_database WHERE datname='vaulthalla'" | grep -q 1 ||
    sudo -u postgres psql -c "CREATE DATABASE vaulthalla OWNER vaulthalla;"

sudo -u postgres psql -c "GRANT ALL PRIVILEGES ON DATABASE vaulthalla TO vaulthalla;"

# === 9) Inject DB Password into Config ===
echo "✍️  Updating config with DB password..."
sudo sed -i "s/^\(\s*password:\s*\).*/\1${VAUL_PG_PASS}/" /etc/vaulthalla/config.yaml

# === 10) Apply Schema + Seed DB ===
for sql_file in auth vaults files acl; do
    echo "📄 Applying $sql_file.sql..."
    sudo -u vaulthalla psql -d vaulthalla -f "deploy/psql/$sql_file.sql"
done

echo "🌱 Seeding database..."
sudo -u vaulthalla psql -d vaulthalla -f deploy/psql/seed.sql

echo "🔐 Set admin password (Enter = vh!adm1n):"
read -rs ADMIN_PLAIN
ADMIN_PLAIN="${ADMIN_PLAIN:-vh!adm1n}"

echo "🔒 Hashing admin password..."
HASHED_PASS=$(./deploy/psql/hash_password "$ADMIN_PLAIN")

cat <<EOF | sudo -u vaulthalla psql -d vaulthalla
-- Insert Admin User if not exists
INSERT INTO users (name, email, password_hash, created_at, is_active)
VALUES ('Admin', 'admin@vaulthalla.dev', '${HASHED_PASS}', NOW(), TRUE)
ON CONFLICT (email) DO NOTHING;

-- Link Super Admin Role
INSERT INTO roles (subject_id, subject_type, scope, role_id)
SELECT u.id, 'user', 'global', r.id
FROM users u, role r
WHERE u.email = 'admin@vaulthalla.dev' AND r.name = 'super_admin'
AND NOT EXISTS (
  SELECT 1 FROM roles
  WHERE subject_id = u.id AND subject_type = 'user' AND scope = 'global' AND role_id = r.id
);

-- Create Admin Group & Link
INSERT INTO groups (name, description)
VALUES ('admin', 'Core admin group')
ON CONFLICT (name) DO NOTHING;

INSERT INTO group_members (group_id, user_id)
SELECT g.id, u.id
FROM groups g, users u
WHERE g.name = 'admin' AND u.email = 'admin@vaulthalla.dev'
AND NOT EXISTS (
  SELECT 1 FROM group_members
  WHERE group_id = g.id AND user_id = u.id
);

-- Create Admin Default Vault if not exists
INSERT INTO vault (type, name, is_active, created_at)
VALUES ('local', 'Admin Default Vault', TRUE, NOW())
ON CONFLICT (name) DO NOTHING;

-- Insert local mount point if not exists
INSERT INTO local (vault_id, mount_point)
SELECT v.id, 'users/admin'
FROM vault v
WHERE v.name = 'Admin Default Vault'
AND NOT EXISTS (
  SELECT 1 FROM local WHERE vault_id = v.id
);

-- Create Admin Default Volume if not exists
INSERT INTO volume (vault_id, name, path_prefix, quota_bytes, created_at)
SELECT v.id, 'Admin Default Volume', 'default', NULL, NOW()
FROM vault v
WHERE v.name = 'Admin Default Vault'
AND NOT EXISTS (
  SELECT 1 FROM volume WHERE vault_id = v.id AND name = 'Admin Default Volume'
);

-- Link volume to admin user if not exists
INSERT INTO volumes (subject_id, subject_type, volume_id)
SELECT u.id, 'user', vol.id
FROM users u, volume vol
WHERE u.email = 'admin@vaulthalla.dev' AND vol.name = 'Admin Default Volume'
AND NOT EXISTS (
  SELECT 1 FROM volumes
  WHERE subject_id = u.id AND subject_type = 'user' AND volume_id = vol.id
);
EOF

# === 11) Install systemd services ===
echo "🛠️  Installing systemd services..."
sudo install -m 644 deploy/systemd/vaulthalla-core.service /etc/systemd/system/
sudo install -m 644 deploy/systemd/vaulthalla-fuse.service /etc/systemd/system/

sudo systemctl daemon-reload
sudo systemctl enable --now vaulthalla-core.service
sudo systemctl enable --now vaulthalla-fuse.service

echo ""
echo "🏁 Vaulthalla installed successfully!"
