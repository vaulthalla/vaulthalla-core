if mountpoint -q /mnt/vaulthalla; then
  echo "Unmounting stale FUSE mount..."
  sudo umount -l /mnt/vaulthalla || sudo fusermount3 -u /mnt/vaulthalla
fi
