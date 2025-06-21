#!/usr/bin/env bash
set -euo pipefail

INSTALL_DEPS=false

# Check for --install-deps flag
for arg in "$@"; do
    if [[ "$arg" == "--install-deps" ]]; then
        INSTALL_DEPS=true
    fi
done

if $INSTALL_DEPS; then
    echo "📦 Installing required dependencies..."

    # Install PostgreSQL if not present
    if ! command -v psql &>/dev/null; then
        echo "🔌 Installing PostgreSQL..."
        sudo apt update
        sudo apt install -y postgresql
    else
        echo "✅ PostgreSQL already installed."
    fi

    # Install Conan if missing
    if ! command -v conan &>/dev/null; then
        echo "🧱 Installing Conan..."
        pip install --user conan || sudo pip install conan
    else
        echo "✅ Conan already installed."
    fi

    # Install Meson if missing
    if ! command -v meson &>/dev/null; then
        echo "🧰 Installing Meson build system..."
        sudo apt install -y meson ninja-build
    else
        echo "✅ Meson already installed."
    fi
fi

echo "🏗️  Starting Vaulthalla installation..."

# === 0) Conan Remote Setup ===
if ! conan remote list | grep -q "vaulthalla"; then
    echo "📦 Adding Conan remote for Vaulthalla packages..."
    conan remote add vaulthalla https://repo.cooperhlarson.com/artifactory/api/conan/vh-conan-virtual
else
    echo "📦 Conan remote 'vaulthalla' already exists."
fi

# === 1) Install dependencies & build ===
echo "🔧 Installing Conan dependencies..."
conan install . -r vaulthalla --build=missing

echo "🔨 Building binaries..."
conan build .

# === 2) Move helper binary ===
echo "📁 Deploying hash_password helper..."
sudo mv build/hash_password deploy/psql/

# === 3) Create system user + group ===
if ! id vaulthalla &>/dev/null; then
    echo "👤 Creating system user 'vaulthalla'..."
    sudo useradd -r -s /usr/sbin/nologin vaulthalla
else
    echo "👤 System user 'vaulthalla' already exists."
fi

# === 4) Layout essential directories ===
echo "📁 Creating runtime directories..."
sudo install -d -o vaulthalla -g vaulthalla -m 755 /mnt/vaulthalla
sudo install -d -o vaulthalla -g vaulthalla -m 755 /var/lib/vaulthalla
sudo install -d -o vaulthalla -g vaulthalla -m 750 /var/log/vaulthalla
sudo install -d -o vaulthalla -g vaulthalla -m 755 /run/vaulthalla

# === 5) Install binaries ===
echo "🚀 Installing core executables..."
sudo install -d -o vaulthalla -g vaulthalla -m 755 /usr/local/bin/vaulthalla
sudo install -m 755 build/fuse_daemon /usr/local/bin/vaulthalla/
sudo install -m 755 build/core_daemon /usr/local/bin/vaulthalla/

# === 6) Copy default config ===
echo "⚙️  Deploying default config..."
sudo install -d -m 755 /etc/vaulthalla

if [[ -f ./config.yaml ]]; then
    echo "📄 Using local ./config.yaml"
    sudo cp ./config.yaml /etc/vaulthalla/config.yaml
else
    echo "📄 No local config found. Using example config from deploy/"
    sudo cp deploy/config.example.yaml /etc/vaulthalla/config.yaml
fi

# === 7) Create DB user and database ===
VAUL_PG_PASS=$(uuidgen)
echo "🔐 Creating PostgreSQL user and database..."

# Create user if not exists
if ! sudo -u postgres psql -tAc "SELECT 1 FROM pg_roles WHERE rolname='vaulthalla'" | grep -q 1; then
    sudo -u postgres psql -c "CREATE USER vaulthalla WITH PASSWORD '${VAUL_PG_PASS}';"
else
    echo "👤 PostgreSQL user 'vaulthalla' already exists."
fi

# Create database if not exists
if ! sudo -u postgres psql -tAc "SELECT 1 FROM pg_database WHERE datname='vaulthalla'" | grep -q 1; then
    sudo -u postgres psql -c "CREATE DATABASE vaulthalla OWNER vaulthalla;"
else
    echo "🗃️  Database 'vaulthalla' already exists."
fi

# Grant privileges
sudo -u postgres psql -c "GRANT ALL PRIVILEGES ON DATABASE vaulthalla TO vaulthalla;"

# === 8) Inject DB password into config ===
echo "✍️  Updating config with DB password..."
sudo sed -i "s/^\(\s*password:\s*\).*/\1${VAUL_PG_PASS}/" /etc/vaulthalla/config.yaml

# === 9) Apply DB schema ===
echo "📄 Applying schema.sql..."
sudo -u vaulthalla psql -d vaulthalla -f deploy/psql/schema.sql

# === 10) Seed the database ===
echo "🌱 Seeding the database with default roles and admin..."

# ✅ Run the reduced, pure-SQL seed.sql (roles, permissions, etc.)
sudo -u vaulthalla psql -d vaulthalla -f deploy/psql/seed.sql

# 🔐 Prompt for admin password
echo "🔐 Set admin password (press Enter to use default: vh!adm1n):"
read -rs ADMIN_PLAIN
ADMIN_PLAIN="${ADMIN_PLAIN:-vh!adm1n}"

# 🔒 Hash the password with the helper binary
echo "🔒 Hashing admin password..."
HASHED_PASS=$(./deploy/psql/hash_password "$ADMIN_PLAIN")

# 🚀 Inject the dynamic admin user and linkage SQL
cat <<EOF | sudo -u vaulthalla psql -d vaulthalla
-- Insert Admin User
INSERT INTO users (name, email, password_hash, created_at, is_active)
VALUES ('Admin', 'admin@vaulthalla.lan', '${HASHED_PASS}', NOW(), TRUE);

-- Assign admin role
INSERT INTO user_roles (user_id, role_id)
SELECT users.id, roles.id FROM users, roles
WHERE users.email = 'admin@vaulthalla.lan' AND roles.name = 'Admin';

-- Create admin group
INSERT INTO groups (name, description) VALUES ('admin', 'Core admin group');

-- Add admin to admin group
INSERT INTO group_members (gid, uid)
SELECT groups.id, users.id FROM users, groups
WHERE users.email = 'admin@vaulthalla.lan' AND groups.name = 'admin';

-- Create and link vault
INSERT INTO vaults (type, name, is_active, created_at)
VALUES ('local', 'Admin Local Disk Vault', TRUE, NOW());

INSERT INTO local_disk_vaults (vault_id, mount_point)
SELECT id, '/mnt/vaulthalla/users/admin' FROM vaults WHERE name = 'Admin Local Disk Vault';

INSERT INTO storage_volumes (vault_id, name, path_prefix, quota_bytes, created_at)
SELECT id, 'Admin Local Disk Vault', '/users/admin', NULL, NOW() FROM vaults WHERE name = 'Admin Local Disk Vault';

INSERT INTO user_storage_volumes (user_id, storage_volume_id)
SELECT users.id, storage_volumes.id
FROM users, storage_volumes
WHERE users.email = 'admin@vaulthalla.lan' AND storage_volumes.name = 'Admin Local Disk Vault';

-- Assign all permissions to Admin role
INSERT INTO role_permissions (role_id, permission_id)
SELECT roles.id, permissions.id FROM roles, permissions
WHERE roles.name = 'Admin';
EOF

# === 11) Install systemd services ===
echo "🛠️  Installing systemd services..."

SERVICE_DIR="./deploy/systemd"
SYSTEMD_DIR="/etc/systemd/system"

sudo install -m 644 "$SERVICE_DIR/vaulthalla-core.service" "$SYSTEMD_DIR/"
sudo install -m 644 "$SERVICE_DIR/vaulthalla-fuse.service" "$SYSTEMD_DIR/"

sudo systemctl daemon-reload
sudo systemctl enable --now vaulthalla-core.service
sudo systemctl enable --now vaulthalla-fuse.service

echo "✅ Systemd services installed and enabled."

# === 🎉 DONE ===
echo ""
echo "🏁 Vaulthalla installed successfully!"
