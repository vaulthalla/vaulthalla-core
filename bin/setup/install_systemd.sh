DEV_MODE=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        -d|--dev)
            DEV_MODE=true
            shift
            ;;
    esac
done

echo "🛠️  Installing systemd service..."

# uncomment the EnvironmentFile line in the service file
sudo install -d -m 0755 /etc/systemd/system/vaulthalla.service.d
printf '[Service]\nEnvironmentFile=/etc/vaulthalla/vaulthalla.env\n' \
| sudo tee /etc/systemd/system/vaulthalla.service.d/override.conf >/dev/null

sudo systemctl daemon-reload
sudo systemctl enable --now vaulthalla.service
sudo systemctl enable --now vaulthalla-cli.socket
sudo systemctl enable --now vaulthalla-cli.service

echo ""
echo "🏁 Vaulthalla installed successfully!"

if [[ "$DEV_MODE" == true ]]; then
    sudo journalctl -f -u vaulthalla
fi
