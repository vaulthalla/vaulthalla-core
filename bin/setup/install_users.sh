if ! getent group vaulthalla > /dev/null; then
    echo "👥 Creating 'vaulthalla' group..."
    sudo groupadd --system vaulthalla
else
    echo "👥 'vaulthalla' group already exists."
fi

if ! id vaulthalla &>/dev/null; then
    echo "👤 Creating system user 'vaulthalla'..."
    sudo useradd -r -g vaulthalla -s /usr/sbin/nologin vaulthalla
else
    echo "👤 System user 'vaulthalla' already exists."
fi

# === 3) Add current user to 'vaulthalla' group ===
if [[ -n "${SUDO_USER:-}" ]]; then
    echo "👤 Adding current user '$SUDO_USER' to 'vaulthalla' group..."
    sudo usermod -aG vaulthalla "$SUDO_USER"
    echo "Please log out and back in for group changes to take effect."
else
    echo "⚠️ No SUDO_USER found, skipping user group addition."
fi

# === 4) Add 'vaulthalla' user to 'tss' group (for TPM access) ===
if getent group tss > /dev/null; then
    echo "🔑 Adding 'vaulthalla' to existing 'tss' group..."
    sudo usermod -aG tss vaulthalla
else
    echo "⚠️ No 'tss' group found, creating one..."
    sudo groupadd --system tss
    sudo usermod -aG tss vaulthalla
fi

# === 5) Ensure TPM device permissions ===
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
