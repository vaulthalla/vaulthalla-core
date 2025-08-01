#!/usr/bin/env bash
set -euo pipefail

echo "🗡️  Initiating Vaulthalla uninstallation sequence..."

DEV_MODE=false

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

if mountpoint -q /mnt/vaulthalla; then
  echo "Unmounting stale FUSE mount..."
  sudo umount -l /mnt/vaulthalla || sudo fusermount3 -u /mnt/vaulthalla
fi

# === 1) Stop and disable systemd service (if exists) ===
echo "🗑️  Removing systemd services..."
sudo systemctl stop vaulthalla.service || true
sudo systemctl disable vaulthalla.service || true
sudo rm -f /etc/systemd/system/vaulthalla.service
sudo systemctl daemon-reload
echo "✅ Systemd services uninstalled."

# === 2) Remove binaries ===
echo "🧹 Removing installed binaries..."
sudo rm -rf /usr/local/bin/vaulthalla

# === 3) Remove runtime and config dirs ===
echo "🗑️  Cleaning directories..."

sudo rm -rf /mnt/vaulthalla
sudo rm -rf /var/lib/vaulthalla
sudo rm -rf /var/log/vaulthalla
sudo rm -rf /run/vaulthalla
sudo rm -rf /etc/vaulthalla

# === 4) Remove system user ===
if id vaulthalla &>/dev/null; then
    echo "👤 Removing system user 'vaulthalla'..."
    sudo userdel -r vaulthalla || echo "⚠️  Could not delete home or user not removable"
else
    echo "✅ System user 'vaulthalla' not present."
fi

# === 5) Drop PostgreSQL DB and user ===
echo
if [[ "$DEV_MODE" == true ]]; then
    echo "⚠️  [DEV_MODE] Dropping PostgreSQL DB and user 'vaulthalla'..."
    sudo -u postgres psql <<EOF
REVOKE CONNECT ON DATABASE vaulthalla FROM public;
SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE datname = 'vaulthalla';
DROP DATABASE IF EXISTS vaulthalla;
DROP USER IF EXISTS vaulthalla;
EOF
else
    read -p "⚠️  Drop PostgreSQL database and user 'vaulthalla'? [y/N]: " confirm
    if [[ "$confirm" == "y" || "$confirm" == "Y" ]]; then
        echo "🧨 Dropping PostgreSQL DB and user..."
        sudo -u postgres psql <<EOF
REVOKE CONNECT ON DATABASE vaulthalla FROM public;
SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE datname = 'vaulthalla';
DROP DATABASE IF EXISTS vaulthalla;
DROP USER IF EXISTS vaulthalla;
EOF
    else
        echo "🛡️  Skipping database deletion. You can do it manually with psql if needed."
    fi
fi

# === 6) Uninstall Conan and Meson ===
if [[ "$DEV_MODE" == true ]]; then
    echo
    echo "🚫 Skipping Meson uninstall in DEV_MODE."
else
    echo
    read -p "❓ Uninstall Meson build system and Ninja? [y/N]: " remove_meson
    if [[ "$remove_meson" == "y" || "$remove_meson" == "Y" ]]; then
        echo "🧨 Removing Meson and Ninja..."
        sudo apt remove -y meson ninja-build || echo "⚠️ Could not remove Meson/Ninja"
    else
        echo "✅ Meson left installed."
    fi
fi

# === Done ===
echo ""
echo "✅ Vaulthalla has been uninstalled."
echo "🧙‍♂️ If this was a test install, may your next deployment rise stronger from these ashes."
