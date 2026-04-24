echo "🗑️  Cleaning directories..."

PROGRAM_NAME=vaulthalla

sudo rm -f /etc/nginx/sites-enabled/vaulthalla
sudo rm -f /etc/nginx/sites-available/vaulthalla

for dir in /mnt /var/lib /var/log /run /etc /usr/share /usr/lib ; do
  sudo rm -rf "$dir/$PROGRAM_NAME"
done
