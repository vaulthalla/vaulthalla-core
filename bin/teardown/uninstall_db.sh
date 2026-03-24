DEV_MODE=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        -d|--dev)
            DEV_MODE=true
            shift
            ;;
    esac
done

echo

if ! command -v psql >/dev/null 2>&1 || ! id -u postgres >/dev/null 2>&1; then
    echo "🛡️  PostgreSQL not installed or 'postgres' user missing. Skipping database cleanup."
else
    if [[ "$DEV_MODE" == true ]]; then
        echo "⚠️  [DEV_MODE] Dropping PostgreSQL DB and user 'vaulthalla'..."
        sudo -u postgres psql <<EOF
REVOKE CONNECT ON DATABASE vaulthalla FROM public;
SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE datname = 'vaulthalla';
DROP DATABASE IF EXISTS vaulthalla;
DROP ROLE IF EXISTS vaulthalla;
DROP DATABASE IF EXISTS vh_cli_test;
DROP ROLE IF EXISTS vaulthalla_test;
EOF
    else
        read -p "⚠️  Drop PostgreSQL database and user 'vaulthalla'? [y/N]: " confirm
        if [[ "$confirm" =~ ^[Yy]$ ]]; then
            echo "🧨 Dropping PostgreSQL DB and user..."
            sudo -u postgres psql <<EOF
REVOKE CONNECT ON DATABASE vaulthalla FROM public;
SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE datname = 'vaulthalla';
DROP DATABASE IF EXISTS vaulthalla;
DROP ROLE IF EXISTS vaulthalla;
DROP DATABASE IF EXISTS vh_cli_test;
DROP ROLE IF EXISTS vaulthalla_test;
EOF
        else
            echo "🛡️  Skipping database deletion. You can do it manually with psql if needed."
        fi
    fi
fi