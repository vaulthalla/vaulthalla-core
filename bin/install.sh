#!/usr/bin/env bash
set -euo pipefail

# ═══════════════════════════════════════════════════════
#             ⚔️  VAULTHALLA INSTALLATION ⚔️
#       This script sets up the entire environment
# ═══════════════════════════════════════════════════════

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

# === Patch default Conan profile for modernity ===
echo "🔧 Patching Conan profile: set C++ standard to gnu23 and build_type to Release..."
sudo sed -i 's/compiler\.cppstd=.*/compiler.cppstd=gnu23/' /root/.conan2/profiles/default
sudo sed -i 's/build_type=.*/build_type=Release/' /root/.conan2/profiles/default

# Install dependencies
conan install . --build=missing


echo "🔨 Building binaries..."
conan build .

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
if [[ -f ./config.yaml ]]; then
    echo "📄 Using local ./config.yaml"
    sudo cp ./config.yaml /etc/vaulthalla/config.yaml
else
    echo "📄 Using example config"
    sudo cp deploy/config.example.yaml /etc/vaulthalla/config.yaml
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
echo "📄 Applying schema.sql..."
sudo -u vaulthalla psql -d vaulthalla -f deploy/psql/schema.sql

echo "🌱 Seeding database..."
sudo -u vaulthalla psql -d vaulthalla -f deploy/psql/seed.sql

echo "🔐 Set admin password (Enter = vh!adm1n):"
read -rs ADMIN_PLAIN
ADMIN_PLAIN="${ADMIN_PLAIN:-vh!adm1n}"

echo "🔒 Hashing admin password..."
HASHED_PASS=$(./deploy/psql/hash_password "$ADMIN_PLAIN")

cat <<EOF | sudo -u vaulthalla psql -d vaulthalla
-- Insert Admin User
INSERT INTO users (name, email, password_hash, created_at, is_active)
VALUES ('Admin', 'admin@vaulthalla.dev', '${HASHED_PASS}', NOW(), TRUE);

-- Link Role
INSERT INTO user_roles (user_id, role_id)
SELECT users.id, roles.id FROM users, roles
WHERE users.email = 'admin@vaulthalla.dev' AND roles.name = 'Admin';

-- Create Admin Group & Link
INSERT INTO groups (name, description) VALUES ('admin', 'Core admin group');
INSERT INTO group_members (gid, uid)
SELECT groups.id, users.id FROM users, groups
WHERE users.email = 'admin@vaulthalla.dev' AND groups.name = 'admin';

-- Create Vault
INSERT INTO vaults (type, name, is_active, created_at)
VALUES ('local', 'Admin Local Disk Vault', TRUE, NOW());

INSERT INTO local_disk_vaults (vault_id, mount_point)
SELECT id, '/mnt/vaulthalla/users/admin' FROM vaults WHERE name = 'Admin Local Disk Vault';

INSERT INTO storage_volumes (vault_id, name, path_prefix, quota_bytes, created_at)
SELECT id, 'Admin Local Disk Vault', '/users/admin', NULL, NOW() FROM vaults WHERE name = 'Admin Local Disk Vault';

INSERT INTO user_storage_volumes (user_id, storage_volume_id)
SELECT users.id, storage_volumes.id
FROM users, storage_volumes
WHERE users.email = 'admin@vaulthalla.dev' AND storage_volumes.name = 'Admin Local Disk Vault';

-- Full Permissions
INSERT INTO role_permissions (role_id, permission_id)
SELECT roles.id, permissions.id FROM roles, permissions
WHERE roles.name = 'Admin';
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
