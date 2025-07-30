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

# === 2) Install Build Dependencies ===

./bin/setup/install_build_deps.sh

# === 3) Build Project ===
echo "🏗️  Starting Vaulthalla build..."

meson setup build
meson compile -C build
sudo meson install -C build
sudo ldconfig

# === 4) Setup Runtime Directories ===
echo "📁 Creating runtime directories..."
for dir in /mnt/vaulthalla /var/lib/vaulthalla /var/log/vaulthalla /run/vaulthalla; do
    sudo install -d -o vaulthalla -g vaulthalla -m 755 "$dir"
done
sudo chmod 750 /var/log/vaulthalla

# === 5) Install Binaries ===
echo "🚀 Installing core executables..."
sudo install -d -o vaulthalla -g vaulthalla -m 755 /usr/local/bin/vaulthalla
sudo install -m 755 build/vaulthalla /usr/local/bin/vaulthalla/

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

# === 10) Install systemd service ===
echo "🛠️  Installing systemd service..."
sudo install -m 644 deploy/systemd/vaulthalla.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now vaulthalla.service

echo ""
echo "🏁 Vaulthalla installed successfully!"

if [[ "$DEV_MODE" == true ]]; then
    sudo journalctl -f -u vaulthalla
fi
