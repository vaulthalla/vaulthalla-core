#!/usr/bin/env bash
set -euo pipefail

echo "🗡️  Initiating Vaulthalla uninstallation sequence..."

# === 1) Stop and disable systemd service (if exists) ===
echo "🗑️  Removing systemd services..."

# Stop services if running
sudo systemctl stop vaulthalla-fuse.service || true
sudo systemctl stop vaulthalla-core.service || true

# Disable services
sudo systemctl disable vaulthalla-fuse.service || true
sudo systemctl disable vaulthalla-core.service || true

# Remove service files
sudo rm -f /etc/systemd/system/vaulthalla-core.service
sudo rm -f /etc/systemd/system/vaulthalla-fuse.service

# Reload systemd daemon
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

# === 5) Prompt to drop the PostgreSQL DB and user ===
echo
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

# === Done ===
echo ""
echo "✅ Vaulthalla has been uninstalled."
echo "🧙‍♂️ If this was a test install, may your next deployment rise stronger from these ashes."
