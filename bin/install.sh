#!/usr/bin/env bash
set -euo pipefail

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
echo "🔐 Generating PostgreSQL user and database..."

sudo -u postgres psql <<EOF
DO \$\$
BEGIN
    IF NOT EXISTS (SELECT FROM pg_roles WHERE rolname = 'vaulthalla') THEN
        CREATE USER vaulthalla WITH PASSWORD '${VAUL_PG_PASS}';
    END IF;

    IF NOT EXISTS (SELECT FROM pg_database WHERE datname = 'vaulthalla') THEN
        CREATE DATABASE vaulthalla OWNER vaulthalla;
    END IF;
END
\$\$;
GRANT ALL PRIVILEGES ON DATABASE vaulthalla TO vaulthalla;
EOF

# === 8) Inject DB password into config ===
echo "✍️  Updating config with DB password..."
sudo sed -i "s/^\(password:\s*\).*\(#.*\)/\1${VAUL_PG_PASS} \2/" /etc/vaulthalla/config.yaml

# === 9) Apply DB schema ===
echo "📄 Applying schema.sql..."
psql -U vaulthalla -d vaulthalla -f deploy/psql/schema.sql

# === 10) Seed the database ===
echo "🌱 Seeding the database with default roles and admin..."
psql -U vaulthalla -d vaulthalla -f deploy/psql/seed.sql

# === 11) Install systemd services ===
echo "🛠️  Installing systemd services..."

SERVICE_DIR="./deploy/systemd"
SYSTEMD_DIR="/etc/systemd/system"

# Core and FUSE services
sudo install -m 644 "$SERVICE_DIR/vaulthalla-core.service" "$SYSTEMD_DIR/"
sudo install -m 644 "$SERVICE_DIR/vaulthalla-fuse.service" "$SYSTEMD_DIR/"

# Reload systemd daemon and enable both
sudo systemctl daemon-reload
sudo systemctl enable --now vaulthalla-core.service
sudo systemctl enable --now vaulthalla-fuse.service

echo "✅ Systemd services installed and enabled (not started yet)."

# === 🎉 DONE ===
echo ""
echo "🏁 Vaulthalla installed successfully!"
echo "💡 Tip: Enable and start the service with:"
echo "    sudo systemctl enable --now vaulthalla.service"
