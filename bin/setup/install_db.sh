DEV_MODE=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        -d|--dev)
            DEV_MODE=true
            shift
            ;;
    esac
done

VAUL_PG_PASS=$(openssl rand -base64 32 | tr -d '\n')
echo "🔐 Creating PostgreSQL user and database..."

sudo -u postgres psql -tc "SELECT 1 FROM pg_roles WHERE rolname='vaulthalla'" | grep -q 1 ||
  sudo -u postgres psql -c "CREATE USER vaulthalla WITH PASSWORD '${VAUL_PG_PASS}';"

sudo -u postgres psql -tc "SELECT 1 FROM pg_database WHERE datname='vaulthalla'" | grep -q 1 ||
  sudo -u postgres psql -c "CREATE DATABASE vaulthalla OWNER vaulthalla;"

sudo -u postgres psql -c "GRANT ALL PRIVILEGES ON DATABASE vaulthalla TO vaulthalla;"
sudo -u postgres psql -d vaulthalla -c "GRANT USAGE, CREATE ON SCHEMA public TO vaulthalla;"

# === Seed DB Password (for runtime TPM sealing) ===
PENDING_DB_PASS_FILE="/run/vaulthalla/db_password"
echo "🔐 Seeding PostgreSQL password for Vaulthalla user (pending runtime init)..."
echo "$VAUL_PG_PASS" | sudo tee "$PENDING_DB_PASS_FILE" >/dev/null
sudo chown vaulthalla:vaulthalla "$PENDING_DB_PASS_FILE"
sudo chmod 600 "$PENDING_DB_PASS_FILE"
echo "✅ Wrote DB password seed to $PENDING_DB_PASS_FILE"

if [[ "$DEV_MODE" == true ]]; then
  VAUL_TEST_PG_PASS=$(openssl rand -base64 32 | tr -d '\n')

  sudo -u postgres psql -tc "SELECT 1 FROM pg_roles WHERE rolname='vaulthalla_test'" | grep -q 1 ||
    sudo -u postgres psql -c "CREATE ROLE vaulthalla_test WITH LOGIN PASSWORD '${VAUL_TEST_PG_PASS}';"

  sudo -u postgres psql -tc "SELECT 1 FROM pg_database WHERE datname='vh_cli_test'" | grep -q 1 ||
    sudo -u postgres psql -c "CREATE DATABASE vh_cli_test OWNER vaulthalla_test;"

  sudo -u postgres psql -c "GRANT ALL PRIVILEGES ON DATABASE vh_cli_test TO vaulthalla_test;"
  sudo -u postgres psql -d vh_cli_test -c "GRANT USAGE, CREATE ON SCHEMA public TO vaulthalla_test;"

  # 🔥 Persist to bashrc (replace if exists, append if not)
  if grep -q "^export VH_TEST_DB_PASS=" "$VAULTHALLA_BASHRC_PATH"; then
    sed -i "s|^export VH_TEST_DB_PASS=.*|export VH_TEST_DB_PASS=\"${VAUL_TEST_PG_PASS}\"|" "$VAULTHALLA_BASHRC_PATH"
  else
    echo "export VH_TEST_DB_PASS=\"${VAUL_TEST_PG_PASS}\"" >> "$VAULTHALLA_BASHRC_PATH"
  fi

  # 🔥 Export for current session
  export VH_TEST_DB_PASS="${VAUL_TEST_PG_PASS}"
fi
