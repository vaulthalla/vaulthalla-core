#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "$0")/../lib/dev_mode.sh"

vh_require_dev_mode

if ! command -v psql >/dev/null 2>&1 || ! id -u postgres >/dev/null 2>&1; then
    echo "🛡️  PostgreSQL not installed or 'postgres' user missing. Skipping database cleanup."
    exit 0
fi

if vh_confirm "⚠️  Drop PostgreSQL database and user 'vaulthalla_test'?" "N" "Y"; then
    if vh_is_dev_mode; then
        echo "⚠️  [DEV_MODE] Dropping PostgreSQL DB and user 'vaulthalla_test'..."
    else
        echo "🧨 Dropping PostgreSQL DB and user..."
    fi

    sudo -u postgres psql <<EOF
REVOKE CONNECT ON DATABASE vh_cli_test FROM public;
SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE datname = 'vh_cli_test';
DROP DATABASE IF EXISTS vh_cli_test;
DROP ROLE IF EXISTS vaulthalla_test;
EOF
else
    echo "🛡️  Skipping database deletion. You can do it manually with psql if needed."
fi
