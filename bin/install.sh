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

# === 1) Install Build Dependencies ===
./bin/setup/install_deps.sh

# === 2) Create System User and Group ===
if ! id vaulthalla &>/dev/null; then
    echo "👤 Creating system user 'vaulthalla'..."
    sudo useradd -r -s /usr/sbin/nologin vaulthalla
else
    echo "👤 System user 'vaulthalla' already exists."
fi

# Check if 'tss' group exists and add 'vaulthalla' to it
if getent group tss > /dev/null; then
    echo "🔑 Adding 'vaulthalla' to existing 'tss' group..."
    sudo usermod -aG tss vaulthalla
else
    echo "⚠️ No 'tss' group found, creating one..."
    sudo groupadd --system tss
    sudo usermod -aG tss vaulthalla
fi

# === 3) Ensure TPM device permissions ===
udev_rule_file="/etc/udev/rules.d/60-tpm.rules"

check_perms() {
    local dev=$1
    if [ -e "$dev" ]; then
        perms=$(stat -c "%a %G" "$dev")
        if [[ "$perms" != "660 tss" ]]; then
            return 1
        fi
    fi
    return 0
}

if ! check_perms /dev/tpm0 || ! check_perms /dev/tpmrm0; then
    echo "🔧 Fixing TPM device permissions via udev rule..."
    cat <<'EOF' | sudo tee "$udev_rule_file" >/dev/null
KERNEL=="tpm[0-9]*",   GROUP="tss", MODE="0660"
KERNEL=="tpmrm[0-9]*", GROUP="tss", MODE="0660"
EOF
    sudo udevadm control --reload-rules
    sudo udevadm trigger
else
    echo "✅ TPM device permissions already correct."
fi

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

if [[ "$DEV_MODE" == true ]]; then
    sudo cp ~/vh/vaulthalla.env /etc/vaulthalla/
    sudo chown -R vaulthalla:vaulthalla /etc/vaulthalla/
    sudo chmod 644 /etc/vaulthalla/vaulthalla.env
else
    echo "🔒 Production mode enabled."
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

# === 9) Install systemd service ===
echo "🛠️  Installing systemd service..."
sudo install -m 644 deploy/systemd/vaulthalla.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now vaulthalla.service

echo ""
echo "🏁 Vaulthalla installed successfully!"

if [[ "$DEV_MODE" == true ]]; then
    sudo journalctl -f -u vaulthalla
fi
