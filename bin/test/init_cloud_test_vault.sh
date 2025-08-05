#!/bin/bash

cat <<EOF | sudo -u vaulthalla psql -d vaulthalla
-- Set user linux_uid for testing
UPDATE users
SET linux_uid = 1000
WHERE name = 'admin';

-- Insert a test API key for Cloudflare R2
INSERT INTO api_keys (user_id, name, provider, access_key,
                      encrypted_secret_access_key, iv,
                      region, endpoint)
VALUES (
    (SELECT id FROM users WHERE name = 'admin'),
    'R2 Test Key',
    'Cloudflare R2',
    '${VAULTHALLA_TEST_R2_ACCESS_KEY}',
    decode('00000000000000000000000000000000', 'hex'), -- placeholder encrypted secret
    decode('000000000000000000000000', 'hex'),        -- placeholder IV
    'wnam',
    '${VAULTHALLA_TEST_R2_ENDPOINT}'
)
ON CONFLICT (name) DO NOTHING;

-- Create test vault for R2
INSERT INTO vault (type, name, mount_point, is_active, created_at, owner_id, description)
VALUES (
    's3', 'R2 Test Vault', 'cloud/r2_test_vault', TRUE, NOW(),
    (SELECT id FROM users WHERE name = 'admin'),
    'Test vault for Cloudflare R2 integration'
)
ON CONFLICT (name) DO NOTHING;

-- Create sync entry for R2 Test Vault
WITH ins AS (
    INSERT INTO sync (vault_id, interval)
    VALUES(
        (SELECT id FROM vault WHERE name = 'R2 Test Vault'),
        600
    ) RETURNING id
)
INSERT INTO rsync (sync_id, conflict_policy, strategy)
SELECT id, 'keep_local', 'sync' FROM ins;

-- Create test bucket for R2
INSERT INTO s3_buckets (name, api_key_id)
VALUES (
    'vaulthalla-test',
    (SELECT id FROM api_keys WHERE name = 'R2 Test Key')
);

-- Insert s3 mount point if not exists
INSERT INTO s3 (vault_id, bucket_id)
SELECT v.id, b.id
FROM vault v, s3_buckets b
WHERE v.name = 'R2 Test Vault'
AND b.name = 'vaulthalla-test'
AND NOT EXISTS (
  SELECT 1 FROM s3 WHERE vault_id = v.id AND bucket_id = b.id
);
EOF
